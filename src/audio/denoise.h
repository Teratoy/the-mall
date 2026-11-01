#pragma once

/// The last thing every sound passes through on its way out: everything
/// mixed (sounds, voices, music) is cleaned of the fizz the crush leaves.
/// A crushed sound holds each of its few samples a second as a flat step,
/// and the sharp edges of those steps ring as a hiss far above anything the
/// sound means to say; a steep low-pass just under half the crush's rate
/// smooths them away, keeping the low-bit grit itself. Quiet crushed tails
/// that only flicker between the last steps are faded out by a gentle gate.
namespace denoise {

/// Puts it on raylib's final mix. Call once the audio device is open.
void attach();
void detach();

} // namespace denoise
