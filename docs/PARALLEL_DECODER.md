# Decoder paths

The DSP runs the native GNU Radio receiver and the complete OpenHoshimi
soundmodem core in parallel. The native path uses PFB clock recovery, a Costas
loop, an equalizer and two convolutional phase hypotheses. OpenHoshimi keeps
its own input conditioning, synchronization and CCSDS decoder.

All successful 223-byte FEC frames enter `FrameMonitor`. The first copy is
published immediately through a metadata-free PMT envelope; identical copies
from the other decoder are suppressed for two seconds. There is no arbitration
holding window on valid FEC frames, so this preserves minimum upload latency.

## Retired GNU diversity experiment

The diversity topology was evaluated against
`CLA-179/Lilacsat-soundmodem-CLi`. No source code from that repository is
vendored because the repository currently has no declared software license.
The implementation here uses the existing GNU Radio blocks and dependencies.

The following CLI implementation details were intentionally not carried over:

- Its peak AGC scans all 512 stored samples for every input sample
  (`O(N * 512)`) and reads the ring position after advancing it, producing an
  avoidable delay/indexing error.
- Its `max` value is reset only at the start of each roughly 40 ms block, not
  for each sample, so it is not a true sliding maximum: gain can only fall
  within a block and jumps at the block boundary.
- The ring index is advanced before the output is read, creating about 511
  samples of unintended delay and a zero-filled startup transient. The
  `1e-7` floor avoids division by zero, but permits noise gain as high as
  `1e7`.
- FFT input/output buffers and the FFTW plan are allocated and destroyed for
  every displayed block.
- UI FFT, constellation transport and network output are mixed into the DSP
  loop, making headless and parallel operation unnecessarily expensive.
- Several stream adapters use `std::deque` one sample at a time instead of a
  fixed ring buffer or scheduler-owned buffers.

An earlier build retained a second GNU Radio synchronizer inspired by that
topology. It shared the native DDC/AGC and duplicated FLL, Gardner timing,
Costas, Viterbi and FEC blocks. Once the complete OpenHoshimi core was added,
an 11-recording A/B test found no frame hash unique to this experimental path.
Removing it reduced benchmark CPU time by approximately 35--49% without
changing any final unique-frame set, so it was retired.

The former GNU Radio `feedforward_agc_cc(1024)` was non-causal and scanned the
full window per output sample. At 48 kHz it added about 21.3 ms look-ahead and
roughly 49 million envelope comparisons per second. It has been replaced by a
shared causal `agc2_cc` with attack `0.10`, decay `0.001`, reference `1.0` and
maximum gain `100` (40 dB). The new controller is O(N), has no look-ahead, and
cannot amplify silence/noise without bound.

## Regression snapshot

The benchmark was run on the four `*_TLM.wav` recordings in the project test
set. Counts below were compared by unique frame hashes during the original
integration test.

| Recording | Original only | Parallel enabled |
| --- | ---: | ---: |
| RAW AGC OFF | 72 | 73 |
| RAW AGC ON | 72 | 72 |
| USB AGC OFF, real 12 kHz IF | 73 | 73 |
| USB AGC ON, real 12 kHz IF | 72 | 73 |

The tested set gained two frames in total and had no frame-count regression.
CPU time increased by roughly 10–30% on these short offline runs; wall time was
essentially unchanged because WAV playback and GNU Radio scheduling overlap.
These figures describe the historical GNU diversity experiment. Current builds
use the native receiver plus OpenHoshimi; use `--no-openhoshimi` to benchmark
the native path alone, and `--real-if` for mono 12 kHz IF recordings.

After replacing the 1024-sample feed-forward AGC with the bounded causal AGC,
the same four recordings each produced 73 unique frames. The former parallel
build produced `73/72/73/73`. CPU time fell from approximately
`3.66–4.39 s` to `1.36–1.92 s`, while offline wall time fell from roughly
`1.85 s` to `0.25–0.30 s` on the test host. These measurements make the causal
`0.10/0.001/40 dB` controller the current default.

A later same-binary A/B regression retained the legacy AGC behind the
benchmark-only `--legacy-agc` switch and tested six recordings, including new
56.25 kHz SDR# AF and I/Q baseband captures. Unique frame counts were:

| Recording | Causal AGC | Legacy AGC |
| --- | ---: | ---: |
| RAW AGC OFF | 73 | 73 |
| RAW AGC ON | 73 | 72 |
| USB AGC OFF | 73 | 73 |
| USB AGC ON | 73 | 73 |
| New SDR# AF baseband | 4 | 4 |
| New SDR# I/Q baseband | 4 | 4 |

Across these files the causal AGC reduced CPU time by approximately 47–74%
and offline wall time by about 85%. Parallel-path decoder-hit counts were lower
in several causal runs, but those hits were duplicate copies of frames already
recovered by the primary path; the unique payload set did not regress.

## SNR interpretation

The benchmark also records the primary path's final SVR estimate. On the four
long regression recordings, the new DSP ended at `12.84/11.24/12.70/12.77 dB`;
the old single-path, feed-forward-AGC DSP ended at
`13.53/11.68/13.55/13.07 dB`. Thus the displayed final SVR is about 0.30–0.85
dB lower (mean approximately 0.57 dB) even though unique decoded frames improve
from 289 to 292. This is an estimator/normalization change, not an improvement
or loss in the recording's physical RF SNR. Frame CRC/FEC yield remains the
primary performance criterion.
