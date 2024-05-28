#include <IO/S3Settings.h>

#include <IO/S3Common.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <Interpreters/Context.h>


namespace DB
{

void S3SettingsByEndpoint::loadFromConfig(
    const Poco::Util::AbstractConfiguration & config,
    const std::string & config_prefix,
    const DB::Settings & settings)
{
    std::lock_guard lock(mutex);
    s3_settings.clear();
    if (!config.has(config_prefix))
        return;

    Poco::Util::AbstractConfiguration::Keys config_keys;
    config.keys(config_prefix, config_keys);

    for (const String & key : config_keys)
    {
        const auto key_path = config_prefix + "." + key;
        const auto endpoint_path = key_path + ".endpoint";
        if (config.has(endpoint_path))
        {
            auto endpoint = config.getString(endpoint_path);
            auto auth_settings = S3::AuthSettings(config, settings, /* for_disk_s3 */false, config_prefix);
            auto request_settings = S3::RequestSettings(config, settings, /* for_disk_s3 */false, settings.s3_validate_request_settings, config_prefix);
            s3_settings.emplace(endpoint, S3Settings{std::move(auth_settings), std::move(request_settings)});
        }
    }
}

std::optional<S3Settings> S3SettingsByEndpoint::getSettings(
    const String & endpoint,
    const String & user,
    bool ignore_user) const
{
    std::lock_guard lock(mutex);
    auto next_prefix_setting = s3_settings.upper_bound(endpoint);

    /// Linear time algorithm may be replaced with logarithmic with prefix tree map.
    for (auto possible_prefix_setting = next_prefix_setting; possible_prefix_setting != s3_settings.begin();)
    {
        std::advance(possible_prefix_setting, -1);
        const auto & [endpoint_prefix, settings] = *possible_prefix_setting;
        if (endpoint.starts_with(endpoint_prefix) && (ignore_user || settings.auth_settings.canBeUsedByUser(user)))
            return possible_prefix_setting->second;
    }

    return {};
}

}
