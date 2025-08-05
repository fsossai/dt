#include <iostream>
#include <map>

int main() {
    std::cout << "sizeof(unordered_map<int,int>): " 
              << sizeof(std::map<int, int>) << std::endl;
    return 0;
}