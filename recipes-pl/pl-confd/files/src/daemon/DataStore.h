#ifndef DATASTORE_H
#define DATASTORE_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <type_traits>

#include <SQLiteCpp/SQLiteCpp.h>

#include "Types.h"

/**
 * @brief Configuration data store handler
 *
 * This is a thin wrapper around an sqlite3 database (which in turn is provided by SQLiteCpp) which
 * actually holds all of the configuration data.
 */
class DataStore {
    public:
        DataStore(const std::filesystem::path &dbPath);
        ~DataStore() = default;

        PropertyValue getKey(const std::string_view &name);
        void setKey(const std::string_view &name, const PropertyValue &value);

        size_t deleteKey(const std::string_view &name);
        size_t deleteSubkeys(const std::string_view &namePrefix);

    private:
        /// Name of the metadata table
        constexpr static const char *kMetaTableName{"MetaInfo"};
        /**
         * @brief Current schema version
         *
         * This integer value defines the database schema version. It's expected to be a
         * monotonically increasing value, where numerically higher values indicate newer schemas.
         */
        constexpr static const uint32_t kCurrentSchemaVersion{1};

        /**
         * @brief Property value types
         *
         * This enum defines the numeric values for the `valueType` columnn in the `PropertyKeys`
         * table, which is used to store the keys (names) and types of properties.
         */
        enum class PropertyValueType: uint32_t {
            /// No associated data
            Null                        = 0,
            /// UTF-8 encoded string
            String                      = 1,
            /// Raw, unformatted binary data
            Blob                        = 2,
            /// Unsigned integer
            Integer                     = 3,
            /// Floating point (decimal) number
            Real                        = 4,
        };

        void initSchema();

        bool hasChildren(const std::string_view &keyName);
        std::optional<std::string> getMetaValue(const std::string_view &key);

        void insertKey(const std::string_view &keyName, const PropertyValue &value);
        void updateKey(const uint32_t keyId, const PropertyValueType oldValueType,
                const PropertyValue &newValue);
        void updateKeyTimestamp(const uint32_t keyId);

        /// Get the name of the table containing values of the given type
        constexpr static std::string_view ValueTableName(const PropertyValueType t) {
            switch(t) {
                case PropertyValueType::String:
                    return "PropertyValuesString";
                case PropertyValueType::Blob:
                    return "PropertyValuesBlob";
                case PropertyValueType::Integer:
                    return "PropertyValuesInteger";
                case PropertyValueType::Real:
                    return "PropertyValuesReal";
                default:
                    return "";
            }
        }
        /// Get the property value type enum (to store in the db) for a given value type
        constexpr static inline PropertyValueType TypeForValue(const PropertyValue &val) {
            return std::visit([](auto&& arg) -> PropertyValueType {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    return PropertyValueType::Null;
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    return PropertyValueType::String;
                }
                else if constexpr (std::is_same_v<T, Blob>) {
                    return PropertyValueType::Blob;
                }
                else if constexpr (std::is_same_v<T, uint64_t>) {
                    return PropertyValueType::Integer;
                }
                else if constexpr (std::is_same_v<T, double>) {
                    return PropertyValueType::Real;
                }
                // booleans are stored as integers
                else if constexpr (std::is_same_v<T, bool>) {
                    return PropertyValueType::Integer;
                }
                // any other types get mapped as null as well (in this case, monostate)
                else {
                    return PropertyValueType::Null;
                }
            }, val);
        }

        /**
         * @brief Bind property value to SQL statement
         *
         * @param stmt Statement to bind to
         * @param colName Column name to bind the value to
         * @param value Value to bind
         */
        static inline void BindValue(SQLite::Statement &stmt, const std::string_view &colName,
                const PropertyValue &value) {
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr(std::is_same_v<T, std::string>) {
                    stmt.bind(colName.data(), arg);
                }
                else if constexpr(std::is_same_v<T, Blob>) {
                    stmt.bind(colName.data(), static_cast<const void *>(arg.data()), arg.size());
                }
                else if constexpr(std::is_same_v<T, uint64_t>) {
                    stmt.bind(colName.data(), static_cast<int64_t>(arg));
                }
                else if constexpr(std::is_same_v<T, double>) {
                    stmt.bind(colName.data(), arg);
                }
                else if constexpr(std::is_same_v<T, bool>) {
                    stmt.bind(colName.data(), arg);
                }
                // other types should never get through to here
                else {
                    throw std::logic_error("invalid type for set");
                }
            }, value);
        }

    private:
        /// storage path on disk of the underlying sqlite database
        std::filesystem::path path;

        /// lock guarding access to the db
        std::mutex dbLock;
        /// sqlite database holding data
        std::unique_ptr<SQLite::Database> db;
};

#endif
