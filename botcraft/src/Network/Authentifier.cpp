#include <asio/ip/tcp.hpp>
#include <asio/connect.hpp>
#include <asio/streambuf.hpp>
#include <asio/read_until.hpp>
#include <asio/read.hpp>

#ifdef USE_ENCRYPTION
#include <asio/ssl.hpp>
#include <openssl/sha.h>
#endif

#include <iomanip>
#include <fstream>
#include <sstream>
#include <iostream>

#include "botcraft/Network/Authentifier.hpp"
#include "botcraft/Utilities/Logger.hpp"

#include "botcraft/Utilities/StringUtilities.hpp"

using namespace ProtocolCraft;

namespace Botcraft
{
    const std::string Authentifier::cached_credentials_path = "botcraft_cached_credentials.json";
    const std::string Authentifier::botcraft_app_id = "a0ad834d-e78a-4881-87f6-390aa0f4b283";
    const Json::Value Authentifier::defaultCachedCredentials = {
        { "msa", {
            { "access_token", nullptr },
            { "expires_date", nullptr },
            { "refresh_token", nullptr }
        }},
        { "name", nullptr },
        { "id", nullptr },
        { "mc_token", nullptr },
        { "expires_date", nullptr },
#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
        { "certificates", {
            { "private_key", nullptr },
            { "public_key", nullptr },
            { "expires_date", nullptr },
            { "signature_v1", nullptr },
            { "signature_v2", nullptr }
        }}
#endif
    };

    Authentifier::Authentifier()
    {
        mc_player_uuid_bytes.fill(0);
#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
        key_timestamp = 0;
        rnd = std::mt19937(static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()));
#endif
    }

    Authentifier::~Authentifier()
    {

    }

    const bool Authentifier::AuthMicrosoft(const std::string& login)
    {
#ifndef USE_ENCRYPTION
        return false;
#else
        const Json::Value cached = GetCachedCredentials(login);
        if (!cached.contains("mc_token") || !cached["mc_token"].is_string() ||
            !cached.contains("expires_date") || !cached["expires_date"].is_number() ||
            !cached.contains("name") || !cached["name"].is_string() ||
            !cached.contains("id") || !cached["id"].is_string())
        {
            LOG_WARNING("Missing or malformed cached credentials for Microsoft account, starting auth flow...");
        }
        else if (IsTokenExpired(cached["expires_date"].get<long long int>()))
        {
            LOG_INFO("Cached Minecraft token for Microsoft account expired, starting auth flow...");
        }
        else
        {
            mc_access_token = cached["mc_token"].get_string();
            player_display_name = cached["name"].get_string();
            mc_player_uuid = cached["id"].get_string();
            UpdateUUIDBytes();
            LOG_INFO("Cached Minecraft token for Microsoft account still valid.");

#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
            LOG_INFO("Getting player certificates...");
            std::tie(private_key, public_key, key_signature, key_timestamp) = GetPlayerCertificates(login, mc_access_token);
            if (private_key.empty() || public_key.empty() || key_signature.empty())
            {
                LOG_ERROR("Unable to get player certificates");
                return false;
            }
            LOG_INFO("Player certificates obtained!");
#endif
            return true;
        }

        // This auth flow is directly inspired from https://github.com/maxsupermanhd/go-mc-ms-auth
        LOG_INFO("Trying to get Microsoft access token...");
        const std::string msa_token = GetMSAToken(login);
        if (msa_token.empty())
        {
            LOG_ERROR("Unable to get a microsoft auth token");
            return false;
        }

        LOG_INFO("Trying to get XBL token...");
        const std::string xbl_token = GetXBLToken(msa_token);
        if (xbl_token.empty())
        {
            LOG_ERROR("Unable to get a XBL token");
            return false;
        }
        LOG_INFO("XBL token obtained!");

        LOG_INFO("Trying to get XSTS token...");
        const auto [xsts_token, xsts_userhash] = GetXSTSToken(xbl_token);
        if (xsts_token.empty())
        {
            LOG_ERROR("Unable to get a XSTS token");
            return false;
        }
        LOG_INFO("XSTS token obtained!");

        LOG_INFO("Trying to get MC token...");
        mc_access_token = GetMCToken(login, xsts_token, xsts_userhash);
        if (mc_access_token.empty())
        {
            LOG_ERROR("Unable to get a MC token");
            return false;
        }
        LOG_INFO("MC token obtained! Almost there...");


        // We assume you're using an account owning minecraft so
        // we don't check (and also a bit because it's complicated)
        // If you don't, Botcraft won't work on online mode.
        // But you can buy yourself a copy of the game:
        // https://www.minecraft.net/get-minecraft
        LOG_INFO("Assuming the account owns Minecraft...");

        LOG_INFO("Trying to get MC profile...");
        std::tie(mc_player_uuid, player_display_name) = GetMCProfile(login, mc_access_token);
        if (mc_player_uuid.empty())
        {
            LOG_ERROR("Unable to get a MC profile");
            return false;
        }
        UpdateUUIDBytes();
        LOG_INFO("MC profile obtained!");

#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
        LOG_INFO("Getting player certificates...");
        std::tie(private_key, public_key, key_signature, key_timestamp) = GetPlayerCertificates(login, mc_access_token);
        if (private_key.empty() || public_key.empty() || key_signature.empty())
        {
            LOG_ERROR("Unable to get player certificates");
            return false;
        }
        LOG_INFO("Player certificates obtained!");
#endif

        LOG_INFO("Authentication completed!");

        return true;
#endif
    }

    const bool Authentifier::JoinServer(const std::string& server_id, const std::vector<unsigned char>& shared_secret, const std::vector<unsigned char>& public_key) const
    {
#ifndef USE_ENCRYPTION
        return false;
#else
        if (mc_player_uuid.empty())
        {
            LOG_ERROR("Trying to join a server before authentication");
            return false;
        }

        SHA_CTX sha_context;
        SHA1_Init(&sha_context);

        SHA1_Update(&sha_context, server_id.c_str(), server_id.length());
        SHA1_Update(&sha_context, shared_secret.data(), shared_secret.size());
        SHA1_Update(&sha_context, public_key.data(), public_key.size());

        std::vector<unsigned char> digest(SHA_DIGEST_LENGTH);
        SHA1_Final(digest.data(), &sha_context);

        // Compute minecraft special hexdigest (see https://wiki.vg/Protocol_Encryption#Client)

        bool is_negative = digest[0] & (1 << 7);

        // Take two complement
        if (is_negative)
        {
            // Revert bits
            for (int i = 0; i < digest.size(); ++i)
            {
                digest[i] = ~digest[i];
            }

            // add 1
            int position = static_cast<int>(digest.size()) - 1;
            while (digest[position] == 255 && position > 0)
            {
                digest[position] = 0;
                position -= 1;
            }
            digest[position] += 1;
        }

        // Get hex representation
        std::stringstream ss;
        for (int i = 0; i < digest.size(); ++i)
        {
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(digest[i] & 0xFF);
        }

        std::string server_hash = ss.str();
        // Remove leading 0
        const size_t start = server_hash.find_first_not_of('0');
        if (start != std::string::npos)
        {
            server_hash = server_hash.substr(start);
        }
        else
        {
            server_hash = "";
        }

        if (is_negative)
        {
            server_hash = "-" + server_hash;
        }

        // Prepare the data to send to the server
        const Json::Value data = {
            { "accessToken", mc_access_token },
            { "selectedProfile", mc_player_uuid },
            { "serverId", server_hash}
        };

        const WebRequestResponse post_response = POSTRequest("sessionserver.mojang.com", "/session/minecraft/join",
            "application/json; charset=utf-8", "*/*", "", data.Dump());

        if (post_response.status_code != 204)
        {
            LOG_ERROR("Response returned with status code " << post_response.status_code
                << " (" << post_response.status_message << ") during server join:\n"
                << post_response.response.Dump(4));
            return false;
        }

        return true;
#endif
    }

    const std::string& Authentifier::GetPlayerDisplayName() const
    {
        return player_display_name;
    }

    const std::array<unsigned char, 16>& Authentifier::GetPlayerUUID() const
    {
        return mc_player_uuid_bytes;
    }

#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
    const std::string& Authentifier::GetPrivateKey() const
    {
        return private_key;
    }

    const std::string& Authentifier::GetPublicKey() const
    {
        return public_key;
    }

    const std::string& Authentifier::GetKeySignature() const
    {
        return key_signature;
    }

    const long long int Authentifier::GetKeyTimestamp() const
    {
        return key_timestamp;
    }

#if PROTOCOL_VERSION == 759 /* 1.19 */
    const std::vector<unsigned char> Authentifier::GetMessageSignature(const std::string& message,
        long long int& salt, long long int& timestamp)
#elif PROTOCOL_VERSION == 760 /* 1.19.1/2 */
    const std::vector<unsigned char> Authentifier::GetMessageSignature(const std::string& message,
        const std::vector<unsigned char>& previous_signature, const std::vector<LastSeenMessagesEntry>& last_seen,
        long long int& salt, long long int& timestamp)
#else
    const std::vector<unsigned char> Authentifier::GetMessageSignature(const std::string& message,
        const int message_sent_index, const UUID& chat_session_uuid,
        const std::vector<std::vector<unsigned char>>& last_seen,
        long long int& salt, long long int& timestamp)
#endif
    {
#ifndef USE_ENCRYPTION
        LOG_ERROR("Trying to compute message signature while botcraft was compiled without USE_ENCRYPTION.");
        return {};
#else
        if (mc_player_uuid.empty() || private_key.empty())
        {
            LOG_ERROR("Trying to join a server before authentication");
            return {};
        }

        // Generate random salt and timestamp
        salt = std::uniform_int_distribution<long long int>(std::numeric_limits<long long int>::min(), std::numeric_limits<long long int>::max())(rnd);
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::array<unsigned char, 8> salt_bytes;
        std::array<unsigned char, 8> timestamp_bytes;

        for (int i = 0; i < 8; ++i)
        {
            salt_bytes[i] = static_cast<unsigned char>((salt >> (8 * (7 - i))) & 0xFF);
            // Signature is computed with seconds not milliseconds
            timestamp_bytes[i] = static_cast<unsigned char>(((timestamp / 1000) >> (8 * (7 - i))) & 0xFF);
        }

        std::array<unsigned char, SHA256_DIGEST_LENGTH> signature_hash;
#if PROTOCOL_VERSION == 759 /* 1.19 */
        // Signature is computed with a dumb json instead of the actual string
        const std::string jsoned_message = "{\"text\":\"" + message + "\"}";

        // Compute hash
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, salt_bytes.data(), salt_bytes.size());
        SHA256_Update(&sha256, mc_player_uuid_bytes.data(), mc_player_uuid_bytes.size());
        SHA256_Update(&sha256, timestamp_bytes.data(), timestamp_bytes.size());
        SHA256_Update(&sha256, jsoned_message.data(), jsoned_message.size());
#elif PROTOCOL_VERSION == 760 /* 1.19.1/2 */
        const unsigned char const_byte_70 = 70;

        // Body hash
        std::array<unsigned char, SHA256_DIGEST_LENGTH> body_hash;
        SHA256_CTX body_sha256;
        SHA256_Init(&body_sha256);
        SHA256_Update(&body_sha256, salt_bytes.data(), salt_bytes.size());
        SHA256_Update(&body_sha256, timestamp_bytes.data(), timestamp_bytes.size());
        SHA256_Update(&body_sha256, message.data(), message.size());
        SHA256_Update(&body_sha256, &const_byte_70, 1);
        // All previously seen messages
        for (int i = 0; i < last_seen.size(); ++i)
        {
            SHA256_Update(&body_sha256, &const_byte_70, 1);
            SHA256_Update(&body_sha256, last_seen[i].GetProfileId().data(), last_seen[i].GetProfileId().size());
            SHA256_Update(&body_sha256, last_seen[i].GetLastSignature().data(), last_seen[i].GetLastSignature().size());
        }
        SHA256_Final(body_hash.data(), &body_sha256);


        // Signature hash
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        if (!previous_signature.empty())
        {
            SHA256_Update(&sha256, previous_signature.data(), previous_signature.size());
        }
        SHA256_Update(&sha256, mc_player_uuid_bytes.data(), mc_player_uuid_bytes.size());
        SHA256_Update(&sha256, body_hash.data(), body_hash.size());
#else
        std::array<unsigned char, 4> bytes_1_big_endian;
        std::array<unsigned char, 4> message_sent_index_bytes;
        std::array<unsigned char, 4> message_size_bytes;
        std::array<unsigned char, 4> last_seen_size_bytes;

        for (int i = 0; i < 4; ++i)
        {
            bytes_1_big_endian[i] = static_cast<unsigned char>((1 >> (8 * (3 - i))) & 0xFF);
            message_sent_index_bytes[i] = static_cast<unsigned char>((message_sent_index >> (8 * (3 - i))) & 0xFF);
            message_size_bytes[i] = static_cast<unsigned char>((static_cast<int>(message.size()) >> (8 * (3 - i))) & 0xFF);
            last_seen_size_bytes[i] = static_cast<unsigned char>((static_cast<int>(last_seen.size()) >> (8 * (3 - i))) & 0xFF);
        }

        // Compute hash
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Init(&sha256);
        // Big endian (int)1
        SHA256_Update(&sha256, bytes_1_big_endian.data(), bytes_1_big_endian.size());
        // signed message link
        SHA256_Update(&sha256, mc_player_uuid_bytes.data(), mc_player_uuid_bytes.size());
        SHA256_Update(&sha256, chat_session_uuid.data(), chat_session_uuid.size());
        SHA256_Update(&sha256, message_sent_index_bytes.data(), message_sent_index_bytes.size());
        // signed message body
        SHA256_Update(&sha256, salt_bytes.data(), salt_bytes.size());
        SHA256_Update(&sha256, timestamp_bytes.data(), timestamp_bytes.size());
        SHA256_Update(&sha256, message_size_bytes.data(), message_size_bytes.size());
        SHA256_Update(&sha256, message.data(), message.size());
        SHA256_Update(&sha256, last_seen_size_bytes.data(), last_seen_size_bytes.size());
        for (size_t i = 0; i < last_seen.size(); ++i)
        {
            SHA256_Update(&sha256, last_seen[i].data(), last_seen[i].size());
        }
#endif
        SHA256_Final(signature_hash.data(), &sha256);

        // Extract signature key from PEM string
        // TODO: store RSA key so we don't have to extract it every chat message?
        // it's not that important as you're not supposed to spam chat hundreds of times per second anyway
        RSA* rsa_signature = nullptr;
        const char* c_string = private_key.c_str();
        BIO* keybio = BIO_new_mem_buf((void*)c_string, -1);
        rsa_signature = PEM_read_bio_RSAPrivateKey(keybio, &rsa_signature, NULL, NULL);
        BIO_free(keybio);

        // Compute signature
        const int rsa_signature_size = RSA_size(rsa_signature);
        std::vector<unsigned char> signature(rsa_signature_size);
        unsigned int signature_size;
        RSA_sign(NID_sha256, signature_hash.data(), static_cast<unsigned int>(signature_hash.size()), signature.data(), &signature_size, rsa_signature);
        RSA_free(rsa_signature);
        signature.resize(signature_size);

        return signature;
#endif
    }
#endif

    void Authentifier::UpdateUUIDBytes()
    {
        for (int i = 0; i < 32; i += 2)
        {
            const std::string byte_str = mc_player_uuid.substr(i, 2);
            mc_player_uuid_bytes[i / 2] = static_cast<unsigned char>(std::strtol(byte_str.c_str(), nullptr, 16));
        }
    }

#ifdef USE_ENCRYPTION
    Json::Value Authentifier::GetCachedProfiles() const
    {
        std::ifstream cache_file(cached_credentials_path);
        if (!cache_file.good())
        {
            return {};
        }
        Json::Value cached_content;
        cache_file >> cached_content;
        cache_file.close();

        return cached_content;
    }

    Json::Value Authentifier::GetCachedCredentials(const std::string& login) const
    {
        const Json::Value profiles = GetCachedProfiles();

        if (profiles.size() > 0 &&
            profiles.contains(login) &&
            profiles[login].is_object())
        {
            return profiles[login];
        }

        return defaultCachedCredentials;
    }

    const std::tuple<std::string, std::string, std::string> Authentifier::ExtractMCFromResponse(const Json::Value& response) const
    {
        if (response.contains("error"))
        {
            LOG_ERROR("Error trying to authenticate: " << response["errorMessage"].get_string());
            return { "","","" };
        }

        if (!response.contains("accessToken"))
        {
            LOG_ERROR("Error trying to authenticate, no accessToken returned");
            return { "","","" };
        }

        if (!response.contains("selectedProfile"))
        {
            LOG_ERROR("Error trying to authenticate, no selectedProfile item found");
            return { "","","" };
        }

        const Json::Value& profile = response.get_object().at("selectedProfile");

        if (!profile.contains("name"))
        {
            LOG_ERROR("Error trying to authenticate, no name in selected profile");
            return { "","","" };
        }

        if (!profile.contains("id"))
        {
            LOG_ERROR("Error trying to authenticate, no id in selected profile");
            return { "","","" };
        }

        return { response["accessToken"].get_string(), profile["name"].get_string(), profile["id"].get_string() };
    }

    const bool Authentifier::IsTokenExpired(const long long int& t) const
    {
        return t < std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void Authentifier::WriteCacheFile(const Json::Value& profiles) const
    {
        std::ofstream cached_ofile(cached_credentials_path);
        if (!cached_ofile.is_open())
        {
            return;
        }
        cached_ofile << profiles.Dump(4) << std::endl;
        cached_ofile.close();
    }

    void Authentifier::UpdateCachedMSA(const std::string& login,
        const std::string& access_token, const std::string& refresh_token,
        const long long int& expiration) const
    {
        Json::Value profiles = GetCachedProfiles();

        if (!profiles.contains(login))
        {
            profiles[login] = defaultCachedCredentials;
        }

        if (access_token.empty())
        {
            profiles[login]["msa"]["access_token"] = nullptr;
        }
        else
        {
            profiles[login]["msa"]["access_token"] = access_token;
        }

        if (refresh_token.empty())
        {
            profiles[login]["msa"]["refresh_token"] = nullptr;
        }
        else
        {
            profiles[login]["msa"]["refresh_token"] = refresh_token;
        }

        if (expiration == -1)
        {
            profiles[login]["msa"]["expires_date"] = nullptr;
        }
        else
        {
            profiles[login]["msa"]["expires_date"] = expiration;
        }

        WriteCacheFile(profiles);
    }

    void Authentifier::UpdateCachedMCToken(const std::string& login,
        const std::string& mc_token, const long long int& expiration) const
    {
        Json::Value profiles = GetCachedProfiles();

        if (!profiles.contains(login))
        {
            profiles[login] = defaultCachedCredentials;
        }

        if (mc_token.empty())
        {
            profiles[login]["mc_token"] = nullptr;
        }
        else
        {
            profiles[login]["mc_token"] = mc_token;
        }

        if (expiration == -1)
        {
            profiles[login]["expires_date"] = nullptr;
        }
        else
        {
            profiles[login]["expires_date"] = expiration;
        }

        WriteCacheFile(profiles);
    }

    void Authentifier::UpdateCachedMCProfile(const std::string& login, const std::string& name, const std::string& id) const
    {
        Json::Value profiles = GetCachedProfiles();

        if (!profiles.contains(login))
        {
            profiles[login] = defaultCachedCredentials;
        }

        if (name.empty())
        {
            profiles[login]["name"] = nullptr;
        }
        else
        {
            profiles[login]["name"] = name;
        }

        if (id.empty())
        {
            profiles[login]["id"] = nullptr;
        }
        else
        {
            profiles[login]["id"] = id;
        }

        WriteCacheFile(profiles);
    }

#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
    void Authentifier::UpdateCachedPlayerCertificates(const std::string& login, const std::string& private_k,
        const std::string& public_k, const std::string& signature_v1,
        const std::string& signature_v2, const long long int& expiration) const
    {
        Json::Value profiles = GetCachedProfiles();

        if (!profiles.contains(login))
        {
            profiles[login] = defaultCachedCredentials;
        }

        if (private_k.empty())
        {
            profiles[login]["certificates"]["private_key"] = nullptr;
        }
        else
        {
            profiles[login]["certificates"]["private_key"] = private_k;
        }

        if (public_k.empty())
        {
            profiles[login]["certificates"]["public_key"] = nullptr;
        }
        else
        {
            profiles[login]["certificates"]["public_key"] = public_k;
        }

        if (signature_v1.empty())
        {
            profiles[login]["certificates"]["signature_v1"] = nullptr;
        }
        else
        {
            profiles[login]["certificates"]["signature_v1"] = signature_v1;
        }

        if (signature_v2.empty())
        {
            profiles[login]["certificates"]["signature_v2"] = nullptr;
        }
        else
        {
            profiles[login]["certificates"]["signature_v2"] = signature_v2;
        }

        if (expiration == -1)
        {
            profiles[login]["certificates"]["expires_date"] = nullptr;
        }
        else
        {
            profiles[login]["certificates"]["expires_date"] = expiration;
        }

        WriteCacheFile(profiles);
    }
#endif

    const std::string Authentifier::GetMSAToken(const std::string& login) const
    {
        // Retrieve cached microsoft credentials
        const Json::Value cached = GetCachedCredentials(login);

        // In case there is something wrong in the cached data
        if (!cached.contains("msa") || !cached["msa"].is_object() ||
            !cached["msa"].contains("refresh_token") || !cached["msa"]["refresh_token"].is_string() ||
            !cached["msa"].contains("access_token") || !cached["msa"]["access_token"].is_string() ||
            !cached["msa"].contains("expires_date") || !cached["msa"]["expires_date"].is_number())
        {
            LOG_ERROR("Error trying to get cached Microsoft credentials");
            UpdateCachedMSA(login, "", "", -1);
            LOG_INFO("Starting authentication process...");
            return MSAAuthDeviceFlow(login);
        }

        if (IsTokenExpired(cached["msa"]["expires_date"].get<long long int>()))
        {
            LOG_INFO("Refreshing Microsoft token...");
            std::string refresh_data;
            refresh_data += "client_id=" + botcraft_app_id;
            refresh_data += "&refresh_token=" + cached["msa"]["refresh_token"].get_string();
            refresh_data += "&grant_type=refresh_token";
            refresh_data += "&redirect_uri=https://login.microsoftonline.com/common/oauth2/nativeclient";

            const WebRequestResponse post_response = POSTRequest("login.live.com", "/oauth20_token.srf",
                "application/x-www-form-urlencoded", "*/*", "", refresh_data);

            // If refresh fails, remove the cache
            // file and restart the whole auth flow
            if (post_response.status_code != 200)
            {
                LOG_ERROR("Response returned with status code " << post_response.status_code
                    << " (" << post_response.status_message << ") during Microsoft token refresh:\n"
                    << post_response.response.Dump(4));
                UpdateCachedMSA(login, "", "", -1);
                LOG_WARNING("Failed to refresh token, starting Microsoft authentication process...");
                return MSAAuthDeviceFlow(login);
            }

            const Json::Value& response = post_response.response;

            if (!response.contains("expires_in"))
            {
                LOG_ERROR("Error trying to refresh Microsoft token, no expires_in in response");
                UpdateCachedMSA(login, "", "", -1);
                LOG_WARNING("Failed to refresh token, starting Microsoft authentication process...");
                return MSAAuthDeviceFlow(login);
            }

            if (!response.contains("refresh_token"))
            {
                LOG_ERROR("Error trying to refresh microsoft token, no refresh_token in response");
                UpdateCachedMSA(login, "", "", -1);
                LOG_WARNING("Failed to refresh token, starting Microsoft authentication process...");
                return MSAAuthDeviceFlow(login);
            }

            if (!response.contains("access_token"))
            {
                LOG_ERROR("Error trying to refresh microsoft token, no access_token in response");
                UpdateCachedMSA(login, "", "", -1);
                LOG_WARNING("Failed to refresh token, starting Microsoft authentication process...");
                return MSAAuthDeviceFlow(login);
            }

            UpdateCachedMSA(login, response["access_token"].get_string(),
                response["refresh_token"].get_string(),
                response["expires_in"].get<long long int>() + std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            );

            LOG_INFO("Cached Microsoft token refreshed");

            return response["access_token"].get_string();
        }

        LOG_INFO("Microsoft token obtained from cache");

        return cached["msa"]["access_token"].get_string();
    }

    const std::string Authentifier::MSAAuthDeviceFlow(const std::string& login) const
    {
        std::string auth_data = "";
        auth_data += "client_id=" + botcraft_app_id;
        auth_data += "&scope=XboxLive.signin%20offline_access";

        const WebRequestResponse post_response = POSTRequest("login.microsoftonline.com", "/consumers/oauth2/v2.0/devicecode",
            "application/x-www-form-urlencoded", "*/*", "", auth_data);

        if (post_response.status_code != 200)
        {
            LOG_ERROR("Response returned with status code " << post_response.status_code << " ("
                << post_response.status_message << ") during microsoft authentification:\n"
                << post_response.response.Dump(4));
            return "";
        }

        const Json::Value& auth_response = post_response.response;

        if (!auth_response.contains("interval"))
        {
            LOG_ERROR("Error trying to get microsoft token, no interval in authentication response");
            return "";
        }

        if (!auth_response.contains("message"))
        {
            LOG_ERROR("Error trying to get microsoft token, no message in authentication response");
            return "";
        }

        if (!auth_response.contains("device_code"))
        {
            LOG_ERROR("Error trying to get microsoft token, no device_code in authentication response");
            return "";
        }

        // Display the instructions the user has to follow to authenticate in the console
        std::cout << auth_response["message"].get_string() << std::endl;
        LOG_INFO(auth_response["message"].get_string());

        const long long int pool_interval = auth_response["interval"].get_number<long long int>();
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(pool_interval + 1));

            const std::string check_auth_status_data =
            "client_id=" + botcraft_app_id +
            "&scope=XboxLive.signin%20offline_access" +
            "&grant_type=urn:ietf:params:oauth:grant-type:device_code" +
            "&device_code=" + auth_response["device_code"].get_string();

            const WebRequestResponse post_response = POSTRequest("login.microsoftonline.com", "/consumers/oauth2/v2.0/token",
                "application/x-www-form-urlencoded", "*/*", "", check_auth_status_data);

            const Json::Value& status_response = post_response.response;

            if (post_response.status_code == 400)
            {
                if (!status_response.contains("error"))
                {
                    LOG_ERROR("Unknown error happened during microsoft device authentication process");
                    return "";
                }

                const std::string error = status_response["error"].get_string();

                if (error == "authorization_pending")
                {
                    continue;
                }
                else if (error == "authorization_declined")
                {
                    LOG_ERROR("User declined authorization during microsoft device authentication check");
                    return "";
                }
                else if (error == "expired_token")
                {
                    LOG_ERROR("User took too long to perform device authentication, aborting");
                    return "";
                }
                else if (error == "invalid_grant")
                {
                    if (!status_response.contains("error_description"))
                    {
                        LOG_ERROR("While waiting for microsoft device authentication, token got invalidated (no further information)");
                    }
                    else
                    {
                        LOG_ERROR("While waiting for microsoft device authentication, token got invalidated: " << status_response["error_description"].get_string());
                    }

                    return "";
                }
            }
            else if (post_response.status_code == 200)
            {
                if (!status_response.contains("expires_in"))
                {
                    LOG_ERROR("Error trying to get microsoft token, no expires_in in device authentication status response");
                    return "";
                }

                if (!status_response.contains("refresh_token"))
                {
                    LOG_ERROR("Error trying to get microsoft token, no refresh_token in device authentication status response");
                    return "";
                }

                if (!status_response.contains("access_token"))
                {
                    LOG_ERROR("Error trying to get microsoft token, no access_token in device authentication status response");
                    return "";
                }

                UpdateCachedMSA(login,
                    status_response["access_token"].get_string(),
                    status_response["refresh_token"].get_string(),
                    status_response["expires_in"].get<long long int>() + std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
                );

                LOG_INFO("Newly obtained Microsoft token stored in cache");

                return status_response["access_token"].get_string();
            }
            else
            {
                LOG_ERROR("Response returned with status code " << post_response.status_code << " (" << post_response.status_message << ") during microsoft device authentification check");
                return "";
            }
        }
    }

    const std::string Authentifier::GetXBLToken(const std::string& msa_token) const
    {
        Json::Value request_data = {
            { "Properties", {
                    {"AuthMethod", "RPS"},
                    {"SiteName", "user.auth.xboxlive.com"},
                    {"RpsTicket", "d=" + msa_token}
                }
            },
            { "RelyingParty", "http://auth.xboxlive.com"},
            { "TokenType", "JWT"}
        };

        const WebRequestResponse post_response = POSTRequest("user.auth.xboxlive.com", "/user/authenticate",
            "application/json", "application/json", "", request_data.Dump());

        if (post_response.status_code != 200)
        {
            LOG_ERROR("Response returned with status code " << post_response.status_code
                << " (" << post_response.status_message << ") during XBL authentication:\n"
                << post_response.response.Dump(4));
            return "";
        }

        const Json::Value& response = post_response.response;

        if (!response.contains("Token"))
        {
            LOG_ERROR("Error trying to get XBL token, no Token in authentication response");
            return "";
        }

        return response["Token"].get_string();
    }

    const std::pair<std::string, std::string> Authentifier::GetXSTSToken(const std::string& xbl_token) const
    {
        Json::Value request_data = {
            { "Properties", {
                    {"SandboxId", "RETAIL"},
                    {"UserTokens", { xbl_token } }
                }
            },
            { "RelyingParty", "rp://api.minecraftservices.com/"},
            { "TokenType", "JWT"}
        };

        const WebRequestResponse post_response = POSTRequest("xsts.auth.xboxlive.com", "/xsts/authorize",
            "application/json", "application/json", "", request_data.Dump());

        if (post_response.status_code != 200)
        {
            LOG_ERROR("Response returned with status code " << post_response.status_code
                << " (" << post_response.status_message << ") during XSTS authentication:\n"
                << post_response.response.Dump(4));
            return { "", "" };
        }

        const Json::Value& response = post_response.response;

        if (!response.contains("Token"))
        {
            LOG_ERROR("Error trying to get XSTS token, no Token in authentication response");
            return { "", "" };
        }

        if (!response.contains("DisplayClaims") || !response["DisplayClaims"].contains("xui")
            || !response["DisplayClaims"]["xui"].is_array() || response["DisplayClaims"]["xui"].size() < 1
            || !response["DisplayClaims"]["xui"][0].contains("uhs"))
        {
            LOG_ERROR("Error trying to get XSTS token, no DisplayClaims/xui/0/uhs in authentication response");
            return { "", "" };
        }

        return { response["Token"].get_string(), response["DisplayClaims"]["xui"][0]["uhs"].get_string() };
    }

    const std::string Authentifier::GetMCToken(const std::string& login, const std::string& xsts_token, const std::string& user_hash) const
    {
        Json::Value request_data = {
            { "identityToken", "XBL3.0 x=" + user_hash + ";" + xsts_token }
        };

        const WebRequestResponse post_response = POSTRequest("api.minecraftservices.com", "/authentication/login_with_xbox",
            "application/json", "application/json", "", request_data.Dump());

        if (post_response.status_code != 200)
        {
            LOG_ERROR("Response returned with status code " << post_response.status_code
                << " (" << post_response.status_message << ") during MC authentication:\n"
                << post_response.response.Dump(4));
            return "";
        }

        const Json::Value& response = post_response.response;

        if (!response.contains("access_token"))
        {
            LOG_ERROR("Error trying to get MC token, no access_token in authentication response");
            return "";
        }

        if (!response.contains("expires_in"))
        {
            LOG_WARNING("No expires_in in authentication response of MC");
            // if no expires_in assuming it is one-time, don't need to cache it
            return response["access_token"].get_string();
        }

        UpdateCachedMCToken(login,
            response["access_token"].get_string(),
            response["expires_in"].get<long long int>() + std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        );

        return response["access_token"].get_string();
    }

    const std::pair<std::string, std::string> Authentifier::GetMCProfile(const std::string& login, const std::string& mc_token) const
    {
        const WebRequestResponse get_response = GETRequest("api.minecraftservices.com", "/minecraft/profile",
            "Bearer " + mc_token);

        if (get_response.status_code != 200)
        {
            LOG_ERROR("Response returned with status code " << get_response.status_code << " (" << get_response.status_message << ") during MC profile retrieval");
            return { "", "" };
        }

        const Json::Value& response = get_response.response;

        if (response.contains("errorMessage"))
        {
            LOG_ERROR("Error trying to get MC profile : " << response["errorMessage"].get_string());
            return { "", "" };
        }

        if (!response.contains("id"))
        {
            LOG_ERROR("Error trying to get MC profile, no id in response");
            return { "", "" };
        }

        if (!response.contains("name"))
        {
            LOG_ERROR("Error trying to get MC profile, no name in response");
            return { "", "" };
        }

        UpdateCachedMCProfile(login,
            response["name"].get_string(),
            response["id"].get_string()
        );
        return { response["id"].get_string(), response["name"].get_string() };
    }

#if PROTOCOL_VERSION > 758 /* > 1.18.2 */
    const std::tuple<std::string, std::string, std::string, long long int> Authentifier::GetPlayerCertificates(const std::string& login,
        const std::string& mc_token) const
    {
        // Retrieve cached certificates
        Json::Value cached = GetCachedCredentials(login);

        const bool invalid_cached_values = !cached.contains("certificates") || !cached["certificates"].is_object() ||
            !cached["certificates"].contains("private_key") || !cached["certificates"]["private_key"].is_string() ||
            !cached["certificates"].contains("public_key") || !cached["certificates"]["public_key"].is_string() ||
#if PROTOCOL_VERSION == 759 /* 1.19 */
            !cached["certificates"].contains("signature_v1") || !cached["certificates"]["signature_v1"].is_string() ||
#else
            !cached["certificates"].contains("signature_v2") || !cached["certificates"]["signature_v2"].is_string() ||
#endif
            !cached["certificates"].contains("expires_date") || !cached["certificates"]["expires_date"].is_number();

        if (invalid_cached_values)
        {
            LOG_INFO("Invalid player certificates cached data");
        }

        // Divided by 1000 because this one is stored in ms
        // TODO: investigate why it sometimes wrongly detects expired keys as still valid
        const bool expired = !invalid_cached_values && IsTokenExpired(cached["certificates"]["expires_date"].get<long long int>() / 1000);

        if (expired)
        {
            LOG_INFO("Player certificates expired");
        }

        // In case there is something wrong in the cached data
        if (invalid_cached_values || expired)
        {
            LOG_INFO("Starting player certificates acquisition process...");

            const WebRequestResponse post_response = POSTRequest("api.minecraftservices.com", "/player/certificates",
                "application/json", "application/json", "Bearer " + mc_token, "");

            if (post_response.status_code != 200)
            {
                LOG_ERROR("Response returned with status code " << post_response.status_code
                    << " (" << post_response.status_message << ") during player certificates acquisition:\n"
                    << post_response.response.Dump(4));
                return { "", "", "", 0};
            }

            const Json::Value& response = post_response.response;

            if (!response.contains("keyPair"))
            {
                LOG_ERROR("Error trying to get player certificates, no keyPair in response");
                return { "", "", "", 0};
            }

            if (!response["keyPair"].contains("privateKey"))
            {
                LOG_ERROR("Error trying to get player certificates, no privateKey in response");
                return { "", "", "", 0 };
            }

            if (!response["keyPair"].contains("publicKey"))
            {
                LOG_ERROR("Error trying to get player certificates, no publicKey in response");
                return { "", "", "", 0 };
            }

            if (!response.contains("publicKeySignature"))
            {
                LOG_ERROR("Error trying to get player certificates, no publicKeySignature in response");
                return { "", "", "", 0 };
            }

            if (!response.contains("publicKeySignatureV2"))
            {
                LOG_ERROR("Error trying to get player certificates, no publicKeySignatureV2 in response");
                return { "", "", "", 0 };
            }

            if (!response.contains("expiresAt"))
            {
                LOG_ERROR("Error trying to get player certificates, no expiresAt in response");
                return { "", "", "", 0 };
            }

            // Convert expires date in ISO8601 to ms since UNIX epoch
            const long long int expires_timestamp = Utilities::TimestampMilliFromISO8601(response["expiresAt"].get_string());

            UpdateCachedPlayerCertificates(login, response["keyPair"]["privateKey"].get_string(),
                response["keyPair"]["publicKey"].get_string(), response["publicKeySignature"].get_string(),
                response["publicKeySignatureV2"].get_string(), expires_timestamp
            );

            return { response["keyPair"]["privateKey"].get_string(),
                response["keyPair"]["publicKey"].get_string(),
#if PROTOCOL_VERSION == 759 /* 1.19 */
                response["publicKeySignature"].get_string(),
#else
                response["publicKeySignatureV2"].get_string(),
#endif
                expires_timestamp
            };
        }

        LOG_INFO("Cached player certificates still valid!");
        return { cached["certificates"]["private_key"].get_string(),
            cached["certificates"]["public_key"].get_string(),
        #if PROTOCOL_VERSION == 759 /* 1.19 */
            cached["certificates"]["signature_v1"].get_string(),
#else
            cached["certificates"]["signature_v2"].get_string(),
#endif
            cached["certificates"]["expires_date"].get<long long int>(),
        };
    }
#endif

    const WebRequestResponse Authentifier::WebRequest(const std::string& host, const std::string& raw_request) const
    {
        asio::io_context io_context;

        // Get a list of endpoints corresponding to the server name.
        asio::ip::tcp::resolver resolver(io_context);
        asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, "https");

        asio::ssl::context ctx(asio::ssl::context::sslv23);
        ctx.set_default_verify_paths();
        ctx.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::verify_none);

        asio::ssl::stream<asio::ip::tcp::socket> socket(io_context, ctx);
        socket.set_verify_mode(asio::ssl::verify_none);
        socket.set_verify_callback([](bool, asio::ssl::verify_context&) {return true; });
        SSL_set_tlsext_host_name(socket.native_handle(), host.c_str());
        asio::connect(socket.lowest_layer(), endpoints);
        socket.handshake(socket.client);
        socket.lowest_layer().set_option(asio::ip::tcp::no_delay(true));

        // Send the request
        asio::streambuf request;
        std::ostream request_stream(&request);
        request_stream << raw_request;

        asio::write(socket, request);

        WebRequestResponse web_response;

        // Read the response status line. The response streambuf will automatically
        // grow to accommodate the entire line. The growth may be limited by passing
        // a maximum size to the streambuf constructor.
        asio::streambuf response;
        asio::read_until(socket, response, "\r\n");

        // Check that response is OK.
        std::istream response_stream(&response);
        std::string http_version;
        response_stream >> http_version;
        response_stream >> web_response.status_code;
        std::getline(response_stream, web_response.status_message);

        // Remove any \r in status message
        web_response.status_message.erase(std::remove(web_response.status_message.begin(), web_response.status_message.end(), '\r'),
            web_response.status_message.end());

        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            LOG_ERROR("Invalid response during web request");
            web_response.response = {};
            return web_response;
        }

        // Empty response
        if (web_response.status_code == 204)
        {
            web_response.response = {};
            return web_response;
        }

        // Read the response headers, which are terminated by a blank line.
        asio::read_until(socket, response, "\r\n\r\n");

        // Process the response headers.
        std::string header;
        long long int data_length = -1;
        while (std::getline(response_stream, header) && header != "\r")
        {
            if (header.find("Content-Length: ") == 0)
            {
                data_length = std::stoll(header.substr(16));
            }
        }

        // Write whatever content we already have to output.
        std::stringstream output_stringstream;
        if (response.size() > 0)
        {
            output_stringstream << &response;
        }

        // Read until EOF, writing data to output as we go.
        asio::error_code error;
        while (asio::read(socket, response, asio::transfer_at_least(1), error))
        {
            output_stringstream << &response;
        }
        const std::string raw_response = output_stringstream.str();

        if (error != asio::error::eof && raw_response.size() != data_length)
        {
            LOG_ERROR("Error trying to read web request response, Error:\n " << error);
            web_response.response = {};
        }
        else
        {
            web_response.response = Json::Parse(raw_response);
        }

        return web_response;
    }

    const WebRequestResponse Authentifier::POSTRequest(const std::string& host, const std::string& endpoint,
        const std::string& content_type, const std::string& accept,
        const std::string& authorization, const std::string& data) const
    {
        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        std::string raw_request = "";
        raw_request += "POST " + endpoint + " HTTP/1.1 \r\n";
        raw_request += "Host: " + host + "\r\n";
        raw_request += "User-Agent: C/1.0\r\n";
        raw_request += "Content-Type: " + content_type + " \r\n";
        raw_request += "Accept: " + accept + "\r\n";
        if (!authorization.empty())
        {
            raw_request += "Authorization: " + authorization + "\r\n";
        }
        raw_request += "Content-Length: " + std::to_string(data.length()) + "\r\n";
        raw_request += "Connection: close\r\n\r\n";
        raw_request += data;

        return WebRequest(host, raw_request);
    }

    const WebRequestResponse Authentifier::GETRequest(const std::string& host, const std::string& endpoint, const std::string& authorization) const
    {
        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        std::string raw_request = "";
        raw_request += "GET " + endpoint + " HTTP/1.1 \r\n";
        raw_request += "Host: " + host + "\r\n";
        if (!authorization.empty())
        {
            raw_request += "Authorization: " + authorization + "\r\n";
        }
        raw_request += "User-Agent: C/1.0\r\n";
        raw_request += "Connection: close\r\n\r\n";

        return WebRequest(host, raw_request);
    }
#endif
}
