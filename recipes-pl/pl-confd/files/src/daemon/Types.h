/**
 * @file
 *
 * @brief Shared type definitions
 *
 * Contains definitions of various types used throughout confd.
 */
#ifndef TYPES_H
#define TYPES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

/// Type for a binary blob
using Blob = std::vector<std::byte>;

/**
 * @brief Allowable value types for a configuration key
 *
 * The sub-types are, in order:
 * - monostate (default constructed/nonexistent property)
 * - null (the property exists, and its value is `null`)
 * - UTF-8 string
 * - Unformatted binary data (BLOB)
 * - Unsigned 64-bit integer
 * - Floating point (double precision)
 * - Boolean
 */
using PropertyValue = std::variant<std::monostate, std::nullptr_t, std::string, Blob,
        uint64_t, double, bool>;

#endif
