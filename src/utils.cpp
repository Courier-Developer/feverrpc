#include <feverrpc/utils.hpp>

int login(Login) { return 1; }

vector<char> read_file(string file_name) {
    ifstream file(file_name, ios::binary);
    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);

    streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    vector<char> vec;
    vec.reserve(fileSize);

    vec.insert(vec.begin(), istream_iterator<char>(file),
               istream_iterator<char>());
    file.close();
    return vec;
}

void save_file(string file_name, vector<char> &data) {
    ofstream file(file_name, ios::out | ios::binary);
    file.write((const char *)&data[0], data.size());
    file.close();
}