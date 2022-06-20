#ifndef DATASTORE_H
#define DATASTORE_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

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

    private:
        /// storage path on disk of the underlying sqlite database
        std::filesystem::path path;

        /// lock guarding access to the db
        std::mutex dbLock;
        /// sqlite database holding data
        std::unique_ptr<SQLite::Database> db;
};

#endif
