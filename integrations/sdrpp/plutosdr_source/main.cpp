#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>
#include <iio.h>
#include <ad9361.h>
#include <utils/optionlist.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <regex>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "plutosdr_source",
    /* Description:     */ "PlutoSDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const std::vector<const char*> deviceWhiteList = {
    "PlutoSDR",
    "ANTSDR",
    "LibreSDR"
};

std::string normalizeIioUri(const char* input)
{
    std::string uri = input ? input : "";
    uri.erase(uri.begin(), std::find_if(
        uri.begin(), uri.end(),
        [](unsigned char ch) { return !std::isspace(ch); }));
    uri.erase(std::find_if(
        uri.rbegin(), uri.rend(),
        [](unsigned char ch) { return !std::isspace(ch); }).base(),
        uri.end());
    if (!uri.empty() && uri.find(':') == std::string::npos) {
        uri = "ip:" + uri;
    }
    return uri;
}

class PlutoSDRSourceModule : public ModuleManager::Instance {
public:
    PlutoSDRSourceModule(std::string name) {
        this->name = name;

        // Define valid samplerates
        for (int sr = 1000000; sr <= 61440000; sr += 500000) {
            samplerates.define(sr, getBandwdithScaled(sr), sr);
        }
        samplerates.define(61440000, getBandwdithScaled(61440000.0), 61440000.0);

        // Define valid bandwidths
        bandwidths.define(0, "Auto", 0);
        for (int bw = 1000000.0; bw <= 52000000; bw += 500000) {
            bandwidths.define(bw, getBandwdithScaled(bw), bw);
        }

        // Define gain modes
        gainModes.define("manual", "Manual", "manual");
        gainModes.define("fast_attack", "Fast Attack", "fast_attack");
        gainModes.define("slow_attack", "Slow Attack", "slow_attack");
        gainModes.define("hybrid", "Hybrid", "hybrid");

        // Load the optional manual network URI before enumerating devices.
        config.acquire();
        devDesc = config.conf["device"];
        if (config.conf.contains("manualUri") &&
            config.conf["manualUri"].is_string()) {
            const std::string configuredUri = normalizeIioUri(
                config.conf["manualUri"].get<std::string>().c_str());
            std::snprintf(manualUri, sizeof(manualUri), "%s",
                          configuredUri.c_str());
        }
        config.release();

        // Enumerate devices
        refresh();

        // Select device
        select(devDesc);

        // Register source
        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("PlutoSDR", &handler);
    }

    ~PlutoSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("PlutoSDR");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = true;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    void refresh() {
        // Clear device list
        devices.clear();

        // Parse discovered IIO device descriptions.
        std::regex backendRgx(".+(?=:)", std::regex::ECMAScript);
        std::regex modelRgx("\\(.+(?=\\),)", std::regex::ECMAScript);
        std::regex serialRgx("serial=[0-9A-Za-z]+", std::regex::ECMAScript);

        // Automatic discovery is best-effort. A failed scan must not prevent
        // an explicitly configured network URI from being added below.
        iio_scan_context* sctx = iio_create_scan_context(NULL, 0);
        if (sctx) {
            iio_context_info** ctxInfoList = nullptr;
            const ssize_t count =
                iio_scan_context_get_info_list(sctx, &ctxInfoList);
            if (count >= 0) {
                for (ssize_t i = 0; i < count; i++) {
                    iio_context_info* info = ctxInfoList[i];
                    std::string desc = iio_context_info_get_description(info);
                    std::string duri = iio_context_info_get_uri(info);

                    // If the device is not a plutosdr, don't include it
                    bool isPluto = false;
                    for (const auto type : deviceWhiteList) {
                        if (desc.find(type) != std::string::npos) {
                            isPluto = true;
                            break;
                        }
                    }
                    if (!isPluto) {
                        flog::warn("Ignored IIO device: [{}] {}", duri, desc);
                        continue;
                    }

                    // Extract the backend
                    std::string backend = "unknown";
                    std::smatch backendMatch;
                    if (std::regex_search(duri, backendMatch, backendRgx)) {
                        backend = backendMatch[0];
                    }

                    // Extract the model
                    std::string model = "Unknown";
                    std::smatch modelMatch;
                    if (std::regex_search(desc, modelMatch, modelRgx)) {
                        model = modelMatch[0];
                        int parenthPos = model.find('(');
                        if (parenthPos != std::string::npos) {
                            model = model.substr(parenthPos+1);
                        }
                    }

                    // Extract the serial
                    std::string serial = "unknown";
                    std::smatch serialMatch;
                    if (std::regex_search(desc, serialMatch, serialRgx)) {
                        serial = serialMatch[0].str().substr(7);
                    }

                    // Construct the device name
                    std::string devName = '(' + backend + ") " + model +
                                          " [" + serial + ']';

                    // Skip duplicate devices
                    if (devices.keyExists(desc) ||
                        devices.nameExists(devName) ||
                        devices.valueExists(duri)) {
                        continue;
                    }

                    devices.define(desc, devName, duri);
                }
                iio_context_info_list_free(ctxInfoList);
            }
            else {
                flog::warn("Automatic IIO discovery failed: {}", int(count));
            }
            iio_scan_context_destroy(sctx);
        }
        else {
            flog::error("Failed get scan context");
        }

        // Windows IP discovery often has no mDNS service. Let the operator
        // enter a complete libiio URI such as ip:192.168.2.1.
        const std::string manual = normalizeIioUri(manualUri);
        if (!manual.empty()) {
            iio_context* manualContext =
                iio_create_context_from_uri(manual.c_str());
            if (manualContext) {
                if (!devices.keyExists(manual) &&
                    !devices.valueExists(manual)) {
                    devices.define(manual, "(manual) " + manual, manual);
                }
                iio_context_destroy(manualContext);
            }
            else {
                flog::warn("Manual IIO context unavailable: {}", manual);
            }
        }

#ifdef __ANDROID__
        // On Android, a default IP entry must be made (TODO: This is not ideal since the IP cannot be changed)
        const char* androidURI = "ip:192.168.2.1";
        const char* androidName = "Default (192.168.2.1)";
        devices.define(androidName, androidName, androidURI);
#endif
    }

    void select(const std::string& desc) {
        // If no device is available, give up
        if (devices.empty()) {
            devDesc.clear();
            return;
        }

        // If the requested device is not available, select the first one.
        const std::string selected =
            devices.keyExists(desc) ? desc : devices.key(0);
        devId = devices.keyId(selected);

        // Update URI
        devDesc = selected;
        uri = devices.value(devId);
        status = "Selected: " + uri;

        // TODO: Enumerate capabilities

        // Load defaults
        samplerate = 2000000;
        bandwidth = 2000000;
        gmId = gainModes.keyId("hybrid");
        gain = -1.0f;

        // Load device config
        config.acquire();
        if (config.conf["devices"][devDesc].contains("samplerate")) {
            samplerate = config.conf["devices"][devDesc]["samplerate"];
        }
        if (config.conf["devices"][devDesc].contains("bandwidth")) {
            bandwidth = config.conf["devices"][devDesc]["bandwidth"];
        }
        if (config.conf["devices"][devDesc].contains("gainMode")) {
            // Select given gain mode or default if invalid
            std::string gm = config.conf["devices"][devDesc]["gainMode"];
            if (gainModes.keyExists(gm)) {
                gmId = gainModes.keyId(gm);
            }
            else {
                gmId = gainModes.keyId("hybrid");
            }
        }
        if (config.conf["devices"][devDesc].contains("gain")) {
            gain = config.conf["devices"][devDesc]["gain"];
            gain = std::clamp<int>(gain, -1.0f, 73.0f);
        }
        config.release();

        // Update samplerate ID
        if (samplerates.keyExists(samplerate)) {
            srId = samplerates.keyId(samplerate);
        }
        else {
            srId = 0;
            samplerate = samplerates.value(srId);
        }

        // Update bandwidth ID
        if (bandwidths.keyExists(bandwidth)) {
            bwId = bandwidths.keyId(bandwidth);
        }
        else {
            bwId = 0;
            bandwidth = bandwidths.value(bwId);
        }
    }

    static void menuSelected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->samplerate);
        flog::info("PlutoSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        flog::info("PlutoSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (_this->running) { return; }

        // Accept a bare IP address or hostname and normalize it to a libiio
        // network URI before opening the context.
        const std::string manual = normalizeIioUri(_this->manualUri);
        if (!manual.empty()) {
            std::snprintf(_this->manualUri, sizeof(_this->manualUri), "%s",
                          manual.c_str());
        }
        if (!manual.empty() && _this->uri != manual) {
            iio_context* probe =
                iio_create_context_from_uri(manual.c_str());
            if (!probe) {
                _this->status = "Cannot open IIO URI: " +
                                manual;
                flog::error("{}", _this->status);
                return;
            }
            iio_context_destroy(probe);
            if (!_this->devices.keyExists(manual) &&
                !_this->devices.valueExists(manual)) {
                _this->devices.define(
                    manual, "(manual) " + manual, manual);
            }
            _this->select(manual);
            config.acquire();
            config.conf["manualUri"] = manual;
            config.conf["device"] = manual;
            config.release(true);
        }

        if (_this->devDesc.empty() || _this->uri.empty()) {
            _this->status =
                "No PlutoSDR selected. Enter a full IIO URI and press Run.";
            flog::error("{}", _this->status);
            return;
        }

        // Open context
        _this->ctx = iio_create_context_from_uri(_this->uri.c_str());
        if (_this->ctx == NULL) {
            _this->status = "Could not open Pluto: " + _this->uri;
            flog::error("Could not open pluto ({})", _this->uri);
            return;
        }

        // Get phy and device handle
        _this->phy = iio_context_find_device(_this->ctx, "ad9361-phy");
        if (_this->phy == NULL) {
            _this->status = "IIO context has no ad9361-phy device.";
            flog::error("Could not connect to pluto phy");
            iio_context_destroy(_this->ctx);
            return;
        }
        _this->dev = iio_context_find_device(_this->ctx, "cf-ad9361-lpc");
        if (_this->dev == NULL) {
            _this->status = "IIO context has no cf-ad9361-lpc device.";
            flog::error("Could not connect to pluto dev");
            iio_context_destroy(_this->ctx);
            return;
        }

        // Get RX channels
        _this->rxChan = iio_device_find_channel(_this->phy, "voltage0", false);
        _this->rxLO = iio_device_find_channel(_this->phy, "altvoltage0", true);

        // Enable RX LO and disable TX
        iio_channel_attr_write_bool(iio_device_find_channel(_this->phy, "altvoltage1", true), "powerdown", true);
        iio_channel_attr_write_bool(_this->rxLO, "powerdown", false);

        // Configure RX channel
        iio_channel_attr_write(_this->rxChan, "rf_port_select", "A_BALANCED");
        iio_channel_attr_write_longlong(_this->rxLO, "frequency", round(_this->freq));                              // Freq
        iio_channel_attr_write_bool(_this->rxChan, "filter_fir_en", true);                                          // Digital filter
        iio_channel_attr_write_longlong(_this->rxChan, "sampling_frequency", round(_this->samplerate));             // Sample rate
        if (_this->gmId == _this->gainModes.keyId("manual") &&
            _this->gain >= 0) {
            iio_channel_attr_write_double(_this->rxChan, "hardwaregain",
                                          _this->gain);
        }
        iio_channel_attr_write(_this->rxChan, "gain_control_mode", _this->gainModes.value(_this->gmId).c_str());    // Gain mode
        _this->setBandwidth(_this->bandwidth);
        
        // Configure the ADC filters
        ad9361_set_bb_rate(_this->phy, round(_this->samplerate));

        // Start worker thread
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        _this->status = "Streaming: " + _this->uri;
        flog::info("PlutoSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (!_this->running) { return; }

        // Stop worker thread
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // Close device
        if (_this->ctx != NULL) {
            iio_context_destroy(_this->ctx);
            _this->ctx = NULL;
        }

        flog::info("PlutoSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            // Tune device
            iio_channel_attr_write_longlong(_this->rxLO, "frequency", round(freq));
        }
        flog::info("PlutoSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        SmGui::LeftLabel("IIO URI");
        SmGui::FillWidth();
        if (SmGui::InputText(CONCAT("##_pluto_uri_", _this->name),
                             _this->manualUri,
                             sizeof(_this->manualUri))) {
            config.acquire();
            config.conf["manualUri"] = _this->manualUri;
            config.release(true);
            _this->status =
                "Bare IP/hostname accepted; ip: is added automatically.";
        }
        if (SmGui::Combo("##plutosdr_dev_sel", &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->samplerate);
            config.acquire();
            config.conf["device"] = _this->devices.key(_this->devId);
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_pluto_sr_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->samplerate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->samplerate);
            if (!_this->devDesc.empty()) {
                config.acquire();
                config.conf["devices"][_this->devDesc]["samplerate"] = _this->samplerate;
                config.release(true);
            }
        }

        // Refresh button
        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_pluto_refr_", _this->name))) {
            const std::string manual = normalizeIioUri(_this->manualUri);
            if (!manual.empty()) {
                std::snprintf(_this->manualUri,
                              sizeof(_this->manualUri), "%s",
                              manual.c_str());
                config.acquire();
                config.conf["manualUri"] = manual;
                config.release(true);
            }
            _this->refresh();
            if (!manual.empty() && _this->devices.keyExists(manual)) {
                _this->select(manual);
                config.acquire();
                config.conf["device"] = manual;
                config.release(true);
            }
            else {
                _this->select(_this->devDesc);
            }
            core::setInputSampleRate(_this->samplerate);
        }
        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_pluto_bw_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
            _this->bandwidth = _this->bandwidths.value(_this->bwId);
            if (_this->running) {
                _this->setBandwidth(_this->bandwidth);
            }
            if (!_this->devDesc.empty()) {
                config.acquire();
                config.conf["devices"][_this->devDesc]["bandwidth"] = _this->bandwidth;
                config.release(true);
            }
        }

        SmGui::LeftLabel("Gain Mode");
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_pluto_gainmode_select_", _this->name), &_this->gmId, _this->gainModes.txt)) {
            if (_this->running) {
                iio_channel_attr_write(_this->rxChan, "gain_control_mode", _this->gainModes.value(_this->gmId).c_str());
            }
            if (!_this->devDesc.empty()) {
                config.acquire();
                config.conf["devices"][_this->devDesc]["gainMode"] = _this->gainModes.key(_this->gmId);
                config.release(true);
            }
        }

        SmGui::LeftLabel("Gain");
        if (_this->gmId) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_pluto_gain__", _this->name), &_this->gain, -1.0f, 73.0f, 1.0f, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                iio_channel_attr_write_double(_this->rxChan, "hardwaregain", _this->gain);
            }
            if (!_this->devDesc.empty()) {
                config.acquire();
                config.conf["devices"][_this->devDesc]["gain"] = _this->gain;
                config.release(true);
            }
        }
        if (_this->gmId) { SmGui::EndDisabled(); }
        if (!_this->status.empty()) {
            SmGui::Text(_this->status.c_str());
        }
    }

    void setBandwidth(int bw) {
        if (bw > 0) {
            iio_channel_attr_write_longlong(rxChan, "rf_bandwidth", bw);
        }
        else {
            iio_channel_attr_write_longlong(rxChan, "rf_bandwidth", std::min<int>(samplerate, 52000000));
        }
    }

    static void worker(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        int blockSize = _this->samplerate / 200.0f;

        // Acquire channels
        iio_channel* rx0_i = iio_device_find_channel(_this->dev, "voltage0", 0);
        iio_channel* rx0_q = iio_device_find_channel(_this->dev, "voltage1", 0);
        if (!rx0_i || !rx0_q) {
            flog::error("Failed to acquire RX channels");
            return;
        }

        // Start streaming
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);

        // Allocate buffer
        iio_buffer* rxbuf = iio_device_create_buffer(_this->dev, blockSize, false);
        if (!rxbuf) {
            flog::error("Could not create RX buffer");
            return;
        }

        // Receive loop
        while (true) {
            // Read samples
            iio_buffer_refill(rxbuf);

            // Get buffer pointer
            int16_t* buf = (int16_t*)iio_buffer_first(rxbuf, rx0_i);
            if (!buf) { break; }

            // Convert samples to CF32
            volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, buf, 32768.0f, blockSize * 2);

            // Send out the samples
            if (!_this->stream.swap(blockSize)) { break; };
        }

        // Stop streaming
        iio_channel_disable(rx0_i);
        iio_channel_disable(rx0_q);

        // Free buffer
        iio_buffer_destroy(rxbuf);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    iio_context* ctx = NULL;
    iio_device* phy = NULL;
    iio_device* dev = NULL;
    iio_channel* rxLO = NULL;
    iio_channel* rxChan = NULL;
    bool running = false;

    std::string devDesc = "";
    std::string uri = "";
    char manualUri[256] = "";
    std::string status = "Automatic PlutoSDR scan is active.";

    double freq;
    int samplerate = 4000000;
    int bandwidth = 0;
    float gain = -1;

    int devId = 0;
    int srId = 0;
    int bwId = 0;
    int gmId = 0;

    OptionList<std::string, std::string> devices;
    OptionList<int, double> samplerates;
    OptionList<int, double> bandwidths;
    OptionList<std::string, std::string> gainModes;
};

MOD_EXPORT void _INIT_() {
    json defConf = {};
    defConf["device"] = "";
    defConf["devices"] = {};
    defConf["manualUri"] = "";
    config.setPath(core::args["root"].s() + "/plutosdr_source_config.json");
    config.load(defConf);
    config.enableAutoSave();

    // Reset the configuration if the old format is still used
    config.acquire();
    if (!config.conf.contains("device") || !config.conf.contains("devices")) {
        config.conf = defConf;
        config.release(true);
    }
    else {
        config.release();
    }
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new PlutoSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (PlutoSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
