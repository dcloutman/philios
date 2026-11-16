#include "support/stderr_capture.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace philios::test {

void StderrCapture::FileCloser::operator()(std::FILE* file) const noexcept {
    if (file != nullptr) {
        [[maybe_unused]] const int result = std::fclose(file);
    }
}

StderrCapture::StderrCapture() : file_(std::tmpfile()) {
    if (!file_) {
        throw std::runtime_error("StderrCapture: tmpfile failed");
    }
    std::cerr.flush();
    [[maybe_unused]] const int flushed = std::fflush(stderr);
    savedDescriptor_ = ::dup(STDERR_FILENO);
    if (savedDescriptor_ < 0) {
        throw std::runtime_error("StderrCapture: dup failed");
    }
    if (::dup2(::fileno(file_.get()), STDERR_FILENO) < 0) {
        [[maybe_unused]] const int closed = ::close(savedDescriptor_);
        savedDescriptor_ = -1;
        throw std::runtime_error("StderrCapture: dup2 failed");
    }
}

StderrCapture::~StderrCapture() {
    std::cerr.flush();
    [[maybe_unused]] const int flushed = std::fflush(stderr);
    if (savedDescriptor_ >= 0) {
        [[maybe_unused]] const int restored = ::dup2(savedDescriptor_, STDERR_FILENO);
        [[maybe_unused]] const int closed = ::close(savedDescriptor_);
    }
}

std::string StderrCapture::text() {
    std::cerr.flush();
    [[maybe_unused]] const int flushed = std::fflush(stderr);
    if (std::fseek(file_.get(), 0, SEEK_SET) != 0) {
        throw std::runtime_error("StderrCapture: fseek failed");
    }
    std::string captured;
    std::array<char, 4096> buffer{};
    for (;;) {
        const std::size_t count = std::fread(buffer.data(), 1, buffer.size(), file_.get());
        captured.append(buffer.data(), count);
        if (count < buffer.size()) {
            break;
        }
    }
    if (std::ferror(file_.get()) != 0) {
        throw std::runtime_error("StderrCapture: fread failed");
    }
    return captured;
}

} // namespace philios::test
