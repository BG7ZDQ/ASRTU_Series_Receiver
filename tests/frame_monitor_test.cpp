#include "frame_monitor.h"

#include <gnuradio/blocks/message_debug.h>
#include <gnuradio/top_block.h>
#include <pmt/pmt.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
	std::atomic<int> candidates{0};
	const auto monitor = FrameMonitor::make({}, {}, [&](const auto &) {
		candidates.fetch_add(1, std::memory_order_relaxed);
	});
	const auto local = gr::blocks::message_debug::make();
	const auto graph = gr::make_top_block("frame-monitor-test");
	std::vector<std::uint8_t> payload(223);
	std::vector<std::thread> threads;

	graph->msg_connect(monitor, "out", local, "store");
	graph->start();
	for (std::size_t i = 0; i < payload.size(); ++i)
		payload[i] = std::uint8_t(i);
	const auto message =
	    pmt::cons(pmt::make_dict(),
		      pmt::init_u8vector(payload.size(), payload.data()));
	for (int i = 0; i < 8; ++i) {
		threads.emplace_back([monitor, message, i] {
			monitor->_post(
			    pmt::intern(i & 1 ? "openhoshimi" : "primary"),
			    message);
		});
	}
	for (auto &thread : threads)
		thread.join();

	auto second = payload;
	payload[0] = 0x03;
	payload[1] = 0x22;
	second[0] = 0x20;
	second[1] = 0x52;
	payload[5] = second[5] = 0x2b;
	payload[6] = second[6] = 0x00;
	payload[7] = second[7] = 0x22;
	monitor->_post(
	    pmt::intern("local_openhoshimi"),
	    pmt::cons(pmt::make_dict(),
		      pmt::init_u8vector(payload.size(), payload.data())));
	monitor->_post(
	    pmt::intern("local_original"),
	    pmt::cons(pmt::make_dict(),
		      pmt::init_u8vector(second.size(), second.data())));
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	graph->stop();
	graph->wait();

	bool normalized = false;
	if (local->num_messages() == 1) {
		const auto forwarded = local->get_message(0);
		normalized = pmt::is_pair(forwarded) &&
			     pmt::is_null(pmt::car(forwarded)) &&
			     pmt::equal(pmt::cdr(forwarded), pmt::cdr(message));
	}
	if (!normalized ||
	    monitor->suppressedDuplicateCount() != 7 ||
	    candidates.load() != 2) {
		std::cerr << "Frame monitor arbitration failed\n";
		return 1;
	}
	return 0;
}
