#include <iostream>
#include <random>
#include <string>

std::string generate_random_salt(size_t length) {
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charset_size = sizeof(charset) - 1;

    std::random_device rd;  // Seed for random number engine
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> distribution(0, charset_size - 1);

    std::string salt;
    for (size_t i = 0; i < length; ++i) {
        salt += charset[distribution(generator)];
    }

    return salt;
}

int main() {
    size_t salt_length = 16;  // Length of the salt
    std::string salt = generate_random_salt(salt_length);
    std::cout << "Generated Salt: " << salt << std::endl;
    return 0;
}