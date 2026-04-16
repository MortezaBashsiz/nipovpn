#pragma once

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <yaml-cpp/yaml.h>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Base class used to disable copy construction and copy assignment.
 *
 * @details
 * - Intended to be inherited privately by classes that must not be copied.
 * - Keeps the default constructor available.
 * - Deletes copy constructor and copy assignment operator.
 */
class Uncopyable {
public:
    Uncopyable() = default;

private:
    Uncopyable(const Uncopyable &) = delete;
    Uncopyable &operator=(const Uncopyable &) = delete;
};

/**
 * @brief Prints a message to standard output.
 *
 * @details
 * - Intended for quick debugging output.
 * - Prefixes the message with a fixed marker.
 *
 * @param message Value to be written to `std::cout`.
 */
inline void OUT(const auto &message) {
    std::cout << "OUT OUT OUT OUT OUT OUT OUT OUT : " << message
              << std::endl;
}

/**
 * @brief Simple result structure containing a success flag and message.
 *
 * @details
 * - Commonly used to return operation status along with an explanatory message.
 */
struct BoolStr {
    bool ok;            ///< Indicates whether the operation succeeded.
    std::string message;///< Contains the result or error message.
};

/**
 * @brief Checks whether a file exists.
 *
 * @param name Path to the file.
 * @return `true` if the file exists and is accessible, otherwise `false`.
 */
inline bool fileExists(const std::string &name) {
    std::ifstream file(name.c_str());
    return file.good();
}

/**
 * @brief Converts a Boost.Asio stream buffer into a string.
 *
 * @param buff Reference to the stream buffer.
 * @return String containing the current contents of the buffer.
 */
inline std::string streambufToString(const boost::asio::streambuf &buff) {
    return {boost::asio::buffers_begin(buff.data()),
            boost::asio::buffers_begin(buff.data()) + buff.size()};
}

/**
 * @brief Replaces the contents of a stream buffer with a string.
 *
 * @details
 * - Clears the destination buffer before writing the new string.
 *
 * @param inputStr Source string to copy into the buffer.
 * @param buff Destination stream buffer.
 */
inline void copyStringToStreambuf(const std::string &inputStr,
                                  boost::asio::streambuf &buff) {
    buff.consume(buff.size());
    std::ostream os(&buff);
    os << inputStr;
}

/**
 * @brief Moves all data from one stream buffer to another.
 *
 * @details
 * - Copies the source buffer contents into the target buffer.
 * - Consumes all data from the source buffer after copying.
 *
 * @param source Source stream buffer.
 * @param target Target stream buffer.
 */
inline void moveStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
    source.consume(source.size());
}

/**
 * @brief Copies all data from one stream buffer to another.
 *
 * @details
 * - Leaves the source buffer unchanged.
 *
 * @param source Source stream buffer.
 * @param target Target stream buffer.
 */
inline void copyStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
}

/**
 * @brief Converts a byte array to its hexadecimal string representation.
 *
 * @param data Pointer to the input byte array.
 * @param size Number of bytes in the array.
 * @return Lowercase hexadecimal string representation of the input.
 */
inline std::string hexArrToStr(const unsigned char *data, std::size_t size) {
    std::stringstream tempStr;
    tempStr << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < size; ++i) {
        tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
    }

    return tempStr.str();
}

/**
 * @brief Converts a hexadecimal string to an integer.
 *
 * @param hexString Hexadecimal string input.
 * @return Parsed value as an unsigned integer.
 */
inline unsigned int hexToInt(const std::string &hexString) {
    unsigned short result;
    std::stringstream hexStr(hexString);
    hexStr >> std::hex >> result;
    return result;
}

/**
 * @brief Converts a hexadecimal string into its ASCII string representation.
 *
 * @details
 * - Processes the input two characters at a time.
 * - Each hexadecimal byte is converted into its character equivalent.
 *
 * @param hex Hexadecimal string input.
 * @return Decoded ASCII string.
 */
inline std::string hexToASCII(const std::string &hex) {
    std::string ascii;

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string part = hex.substr(i, 2);
        char ch = static_cast<char>(stoul(part, nullptr, 16));
        ascii += ch;
    }

    return ascii;
}

/**
 * @brief Converts a hexadecimal character into its numeric value.
 *
 * @param c Input character.
 * @return Numeric value of the hexadecimal character, or `0` if invalid.
 */
inline unsigned char charToHex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return 0;
}

/**
 * @brief Converts a hexadecimal string into a byte array.
 *
 * @param hexStr Hexadecimal string input.
 * @return Vector containing the decoded bytes.
 */
inline std::vector<unsigned char> strToHexArr(const std::string &hexStr) {
    std::vector<unsigned char> result(hexStr.size() / 2);

    for (std::size_t i = 0; i < hexStr.size() / 2; ++i) {
        result[i] = 16 * charToHex(hexStr[2 * i]) + charToHex(hexStr[2 * i + 1]);
    }

    return result;
}

/**
 * @brief Converts a stream buffer into a hexadecimal string.
 *
 * @details
 * - First converts the stream buffer into a raw string.
 * - Then converts the raw bytes into a hexadecimal representation.
 *
 * @param buff Source stream buffer.
 * @return Hexadecimal string representation of the buffer contents.
 */
inline std::string hexStreambufToStr(const boost::asio::streambuf &buff) {
    return hexArrToStr(
            reinterpret_cast<const unsigned char *>(streambufToString(buff).c_str()),
            buff.size());
}

/**
 * @brief Splits a string using a delimiter.
 *
 * @param str Input string.
 * @param delimiter Delimiter string.
 * @return Vector containing the split parts.
 */
inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &delimiter) {
    std::vector<std::string> result;
    std::string tempStr = str;

    while (!tempStr.empty()) {
        size_t index = tempStr.find(delimiter);

        if (index != std::string::npos) {
            result.push_back(tempStr.substr(0, index));
            tempStr = tempStr.substr(index + delimiter.size());

            if (tempStr.empty()) {
                result.push_back(tempStr);
            }
        } else {
            result.push_back(tempStr);
            tempStr.clear();
        }
    }

    return result;
}

/**
 * @brief Decodes a Base64-encoded string.
 *
 * @details
 * - Trims trailing padding characters before decoding.
 * - Uses Boost archive iterators for Base64 decoding.
 * - Throws `std::runtime_error` if decoding fails.
 *
 * @param inputStr Base64-encoded input string.
 * @return Decoded binary string.
 */
inline std::string decode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    std::string temp = inputStr;
    boost::algorithm::trim_right_if(temp, boost::is_any_of("="));

    try {
        using binaryFromBase64 = binary_from_base64<std::string::const_iterator>;
        using transformWidth = transform_width<binaryFromBase64, 8, 6>;

        return {transformWidth(temp.begin()), transformWidth(temp.end())};
    } catch (const std::exception &) {
        throw std::runtime_error("Invalid Base64 encoding");
    }
}

/**
 * @brief Encodes a string into Base64 format.
 *
 * @details
 * - Uses Boost archive iterators for Base64 encoding.
 * - Appends the required padding characters (`=`).
 *
 * @param inputStr Input string.
 * @return Base64-encoded string.
 */
inline std::string encode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    using transformWidth = transform_width<std::string::const_iterator, 6, 8>;
    using base64Enc = base64_from_binary<transformWidth>;

    std::string encoded(base64Enc(inputStr.begin()), base64Enc(inputStr.end()));

    std::size_t numPadChars = (3 - inputStr.size() % 3) % 3;
    encoded.append(numPadChars, '=');

    return encoded;
}

/**
 * @brief Encrypts a string using AES-256-CBC.
 *
 * @details
 * - Generates a random IV for each encryption operation.
 * - Prepends the IV to the encrypted output.
 * - Uses OpenSSL EVP APIs for encryption.
 *
 * @param plaintext Input plaintext string.
 * @param key Encryption key.
 * @return A `BoolStr` containing success status and encrypted data or an error message.
 */
inline BoolStr aes256Encrypt(const std::string &plaintext,
                             const std::string &key) {
    BoolStr result{false, "Encryption failed"};

    unsigned char iv[EVP_MAX_IV_LENGTH];
    if (!RAND_bytes(iv, sizeof(iv))) {
        result.message = "IV generation failed";
        return result;
    }

    int ciphertext_len =
            plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc());
    std::vector<unsigned char> ciphertext(ciphertext_len);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           reinterpret_cast<const unsigned char *>(key.c_str()),
                           iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Encryption initialization failed";
        return result;
    }

    int len;
    if (EVP_EncryptUpdate(
                ctx, ciphertext.data(), &len,
                reinterpret_cast<const unsigned char *>(plaintext.c_str()),
                plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Encryption update failed";
        return result;
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Encryption finalization failed";
        return result;
    }
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    std::string final_result(reinterpret_cast<char *>(iv), sizeof(iv));
    final_result.append(reinterpret_cast<char *>(ciphertext.data()),
                        ciphertext_len);

    result.ok = true;
    result.message = final_result;
    return result;
}

/**
 * @brief Decrypts an AES-256-CBC encrypted string containing a prepended IV.
 *
 * @details
 * - Extracts the IV from the beginning of the input.
 * - Decrypts the remaining ciphertext using OpenSSL EVP APIs.
 *
 * @param ciphertext_with_iv Encrypted input containing IV + ciphertext.
 * @param key Decryption key.
 * @return A `BoolStr` containing success status and decrypted plaintext or an error message.
 */
inline BoolStr aes256Decrypt(const std::string &ciphertext_with_iv,
                             const std::string &key) {
    BoolStr result{false, "Decryption failed"};

    unsigned char iv[EVP_MAX_IV_LENGTH];
    std::memcpy(iv, ciphertext_with_iv.c_str(),
                EVP_CIPHER_iv_length(EVP_aes_256_cbc()));

    std::string ciphertext =
            ciphertext_with_iv.substr(EVP_CIPHER_iv_length(EVP_aes_256_cbc()));

    int max_plaintext_len = ciphertext.size();
    std::vector<unsigned char> plaintext(max_plaintext_len);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           reinterpret_cast<const unsigned char *>(key.c_str()),
                           iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Decryption initialization failed";
        return result;
    }

    int len;
    if (EVP_DecryptUpdate(
                ctx, plaintext.data(), &len,
                reinterpret_cast<const unsigned char *>(ciphertext.c_str()),
                ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Decryption update failed";
        return result;
    }
    int plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Decryption finalization failed";
        return result;
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    result.ok = true;
    result.message =
            std::string(reinterpret_cast<char *>(plaintext.data()), plaintext_len);

    return result;
}

/**
 * @brief Validates command-line arguments and the configuration file.
 *
 * @details
 * - Verifies the expected number of arguments.
 * - Ensures the selected mode is either `server` or `agent`.
 * - Checks that the configuration file exists.
 * - Loads and parses the YAML configuration file.
 * - Validates required keys in the following sections:
 *   - `general`
 *   - `log`
 *   - `server`
 *   - `agent`
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return A `BoolStr` containing validation status and an explanatory message.
 */
inline BoolStr validateConfig(int argc, const char *argv[]) {
    BoolStr result{false, "Validation failed"};

    if (argc != 3) {
        result.message =
                "Requires 2 arguments: mode [server|agent] and config file "
                "path\nExample: nipovpn server config.yaml\n";
        return result;
    }

    if (std::string(argv[1]) != "server" && std::string(argv[1]) != "agent") {
        result.message = "First argument must be one of [server|agent]\n";
        return result;
    }

    if (!fileExists(argv[2])) {
        result.message =
                std::string("Config file ") + argv[2] + " does not exist\n";
        return result;
    }

    YAML::Node configYaml;
    try {
        configYaml = YAML::LoadFile(argv[2]);
    } catch (const std::exception &e) {
        result.message =
                std::string("Error parsing config file: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["general"]["token"].as<std::string>();
        configYaml["general"]["fakeUrl"].as<std::string>();
        configYaml["general"]["method"].as<std::string>();
        configYaml["general"]["timeout"].as<unsigned short>();
        configYaml["general"]["chunkHeader"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'general' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["log"]["logFile"].as<std::string>();
        configYaml["log"]["logLevel"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'log' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["server"]["listenIp"].as<std::string>();
        configYaml["server"]["listenPort"].as<unsigned short>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'server' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["agent"]["listenIp"].as<std::string>();
        configYaml["agent"]["listenPort"].as<unsigned short>();
        configYaml["agent"]["serverIp"].as<std::string>();
        configYaml["agent"]["serverPort"].as<unsigned short>();
        configYaml["agent"]["httpVersion"].as<std::string>();
        configYaml["agent"]["userAgent"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'agent' block: ") + e.what() + "\n";
        return result;
    }

    result.ok = true;
    result.message = "OK";
    return result;
}