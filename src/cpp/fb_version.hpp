/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef FB_VERSION_HPP
#define FB_VERSION_HPP

namespace fb {

class VersionInfo {
public:
    static constexpr unsigned FB30 = 0x0300;
    static constexpr unsigned FB40 = 0x0400;
    static constexpr unsigned FB50 = 0x0500;

    explicit VersionInfo(unsigned version) : version_(version) {}

    [[nodiscard]] bool hasTimeouts() const noexcept { return version_ >= FB40; }

    [[nodiscard]] unsigned getVersion() const noexcept { return version_; }

private:
    unsigned version_;
};

} // namespace fb

#endif // FB_VERSION_HPP
