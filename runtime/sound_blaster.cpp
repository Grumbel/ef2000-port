// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "sound_blaster.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace ef2000 {

SoundBlaster16::SoundBlaster16() {
    for (unsigned i = 0x30; i < 0x3c; ++i) mixer_values_[i] = 0xf8;
}

bool SoundBlaster16::handles(uint16_t port) {
    return (port >= 0x220 && port <= 0x22f) ||
           port == 0x20 || port == 0x21 || port == 0xa0 || port == 0xa1 ||
           port == 0x3da || DMA8237::handles(port);
}

uint8_t SoundBlaster16::read(uint16_t port) {
    if (port == 0x20 || port == 0x21 || port == 0xa0 || port == 0xa1)
        return pic_.read(port);
    if (DMA8237::handles(port)) return dma_.read(port);
    if (port == 0x3da) return std::fmod(time_, 1.0 / 60.0) >= 0.015 ? 8 : 0;
    if (port == 0x22f) {
        irq_status_ &= ~2;
        return 0xff;
    }
    if (port == 0x22e) irq_status_ &= ~1;
    if (port == 0x225 && mixer_index_ < mixer_values_.size() &&
        mixer_index_ >= 0x30 && mixer_index_ <= 0x47)
        return mixer_values_[mixer_index_];
    return SB16Probe::read(port);
}

void SoundBlaster16::write(uint16_t port, uint8_t value) {
    if (port == 0x20 || port == 0x21 || port == 0xa0 || port == 0xa1)
        return pic_.write(port, value);
    if (DMA8237::handles(port)) return dma_.write(port, value);
    if (port == 0x226) {
        if (value & 1) {
            ++generation_;
            active_.reset();
            command_.reset();
            arguments_.clear();
            irq_status_ = 0x20;
            pic_.clear(7);
        }
        return SB16Probe::write(port, value);
    }
    if (port == 0x225) {
        if (mixer_index_ == 0x04 || mixer_index_ == 0x22 ||
            mixer_index_ == 0x26 || mixer_index_ == 0x28 ||
            (mixer_index_ >= 0x30 && mixer_index_ <= 0x47)) {
            mixer_values_[mixer_index_] = value;
            return;
        }
        throw std::runtime_error("unsupported SB16 mixer write " + std::to_string(mixer_index_));
    }
    if (port != 0x22c) return SB16Probe::write(port, value);
    if (!initialized_) throw std::runtime_error("SB16 DSP command before reset");
    if (command_) {
        arguments_.push_back(value);
        if (arguments_.size() == needed_) finish_command();
        return;
    }
    switch (value) {
        case 0xe1: data_.push_back(4); data_.push_back(5); break;
        case 0x41: case 0x42:
            command_ = value;
            needed_ = 2;
            arguments_.clear();
            break;
        case 0xb0: case 0xb2: case 0xb4: case 0xb6:
        case 0xc0: case 0xc2: case 0xc4: case 0xc6:
            command_ = value;
            needed_ = 3;
            arguments_.clear();
            break;
        case 0xd0: case 0xd5:
            if (active_) active_->paused = true;
            break;
        case 0xd4: case 0xd6:
            if (active_) {
                active_->paused = false;
                active_->deadline = time_ + active_->duration;
            }
            break;
        case 0xda: case 0xd9:
            if (active_) active_->auto_init = false;
            break;
        case 0xd1: case 0xd3: speaker_ = value == 0xd1; break;
        case 0xd8: data_.push_back(speaker_ ? 0xff : 0); break;
        default:
            throw std::runtime_error("unsupported SB16 DSP command " + std::to_string(value));
    }
}

void SoundBlaster16::finish_command() {
    const uint8_t command = *command_;
    command_.reset();
    const auto args = std::exchange(arguments_, {});
    if (command == 0x41 || command == 0x42) {
        const unsigned rate = unsigned(args[0]) * 256 + args[1];
        if (!rate) throw std::runtime_error("zero SB16 sample rate");
        sample_rate_ = rate;
        return;
    }
    const unsigned mode = args[0];
    if (mode & ~0x30u)
        throw std::runtime_error("unsupported SB16 sample mode " + std::to_string(mode));
    const unsigned bits = (command & 0xf0) == 0xb0 ? 16 : 8;
    const unsigned channels = (mode & 0x20) ? 2 : 1;
    const unsigned units = unsigned(args[2]) * 256 + args[1] + 1;
    const double duration = double(units) / (sample_rate_ * channels);
    active_ = Transfer{bits, channels, sample_rate_, units, bool(mode & 0x10),
                       bool(command & 4), false, duration, time_ + duration};
}

void SoundBlaster16::advance(double now, const DMA8237::MemoryRead& read_memory) {
    time_ = now;
    if (!active_ || active_->paused) return;
    unsigned overdue = 0;
    while (active_ && time_ >= active_->deadline) {
        if (++overdue > 256)
            throw std::runtime_error("SB16 DMA fell over 256 blocks behind guest execution");
        const unsigned channel = active_->bits == 16 ? 5 : 1;
        auto pcm = dma_.transfer(channel, active_->units, read_memory);
        AudioBlock block;
        block.bits = active_->bits;
        block.channels = active_->channels;
        block.signed_pcm = active_->signed_pcm;
        block.rate = active_->rate;
        block.generation = generation_;
        block.pcm = std::move(pcm);
        for (unsigned i = 0; i < 2; ++i) {
            const double master = double(mixer_values_[0x30 + i] >> 3) / 31;
            const double voice = double(mixer_values_[0x32 + i] >> 3) / 31;
            block.gains[i] = master * voice;
        }
        completed_.push_back(std::move(block));
        if (!(irq_status_ & 3)) {
            irq_status_ |= uint8_t(active_->bits == 16 ? 2 : 1);
            pic_.request(7);
        }
        if (active_->auto_init)
            active_->deadline += active_->duration;
        else
            active_.reset();
    }
}

std::vector<AudioBlock> SoundBlaster16::take_completed() {
    return std::exchange(completed_, {});
}

double SoundBlaster16::next_deadline() const {
    return active_ && !active_->paused ? active_->deadline : 0;
}

} // namespace ef2000
