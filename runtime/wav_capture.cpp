// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wav_capture.hpp"

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ef2000 {

WavCapture::WavCapture(std::filesystem::path directory)
    : directory_(std::move(directory)) {
    if (directory_.empty()) throw std::runtime_error("empty WAV capture directory");
    std::filesystem::create_directories(directory_);
}

WavCapture::~WavCapture() {
    try { close(); }
    catch (...) { /* Explicit close() reports errors to the caller. */ }
}

void WavCapture::put16(std::ostream& out, uint16_t value) {
    out.put(char(value));
    out.put(char(value >> 8));
}

void WavCapture::put32(std::ostream& out, uint32_t value) {
    put16(out, uint16_t(value));
    put16(out, uint16_t(value >> 16));
}

void WavCapture::close() {
    if (file_.is_open()) {
        file_.seekp(4);
        put32(file_, file_bytes_ + 36);
        file_.seekp(40);
        put32(file_, file_bytes_);
        file_.close();
        if (!file_) throw std::runtime_error("failed to finalize original PCM WAV capture");
    }
    carry_.clear();
    active_ = false;
}

void WavCapture::begin(const AudioBlock& block) {
    close();
    bits_ = block.bits;
    channels_ = block.channels;
    rate_ = block.rate;
    generation_ = block.generation;
    signed_pcm_ = block.signed_pcm;
    file_bytes_ = 0;
    active_ = true;
}

void WavCapture::open_file() {
    std::ostringstream name;
    name << "audio-" << std::setw(3) << std::setfill('0') << segments_ << ".wav";
    const auto path = directory_ / name.str();
    if (std::filesystem::exists(path))
        throw std::runtime_error("refusing to overwrite original audio capture: " + path.string());
    file_.open(path, std::ios::binary);
    if (!file_) throw std::runtime_error("cannot create original audio capture: " + path.string());
    ++segments_;
    file_.write("RIFF", 4);
    put32(file_, 36);
    file_.write("WAVEfmt ", 8);
    put32(file_, 16);
    put16(file_, 1); // PCM
    put16(file_, uint16_t(channels_));
    put32(file_, rate_);
    put32(file_, rate_ * channels_ * (bits_ / 8));
    put16(file_, uint16_t(channels_ * (bits_ / 8)));
    put16(file_, uint16_t(bits_));
    file_.write("data", 4);
    put32(file_, 0);
    if (!file_) throw std::runtime_error("failed to write original audio WAV header");
}

void WavCapture::push(const AudioBlock& block) {
    if ((block.bits != 8 && block.bits != 16) ||
        (block.channels != 1 && block.channels != 2) ||
        !block.rate || block.rate > 192000)
        throw std::runtime_error("unsupported original PCM WAV format");
    if (!active_ || bits_ != block.bits || channels_ != block.channels ||
        rate_ != block.rate || signed_pcm_ != block.signed_pcm ||
        generation_ != block.generation)
        begin(block);
    std::vector<uint8_t> pcm = std::move(carry_);
    pcm.insert(pcm.end(), block.pcm.begin(), block.pcm.end());
    const size_t frame_size = (bits_ / 8) * channels_;
    const size_t usable = (pcm.size() / frame_size) * frame_size;
    carry_.assign(pcm.begin() + usable, pcm.end());
    if (!usable) return;
    if (!file_.is_open()) open_file();
    if (usable > std::numeric_limits<uint32_t>::max() - 36u - file_bytes_)
        throw std::runtime_error("original PCM WAV would exceed RIFF size limit");
    if (bits_ == 8 && signed_pcm_) {
        for (size_t i = 0; i < usable; ++i) pcm[i] ^= 0x80;
    } else if (bits_ == 16 && !signed_pcm_) {
        for (size_t i = 1; i < usable; i += 2) pcm[i] ^= 0x80;
    }
    file_.write(reinterpret_cast<const char*>(pcm.data()), usable);
    if (!file_) throw std::runtime_error("failed to write original PCM WAV data");
    file_bytes_ += uint32_t(usable);
    bytes_ += usable;
}

} // namespace ef2000
