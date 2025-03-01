#include <iostream>
#include <omp.h>
#include <vector>
#include <string>
#include <fstream>

#include "Skynet.hpp"

using namespace std;

bool isLetter(char c) {
  return 'A' <= c && c <= 'Z' || 'a' <= c && c <= 'z';
}

int main(int argc, char *argv[]) {
  skynet::HSequence<char> letters_ref;
  skynet::HSequence<char> letters;

  ifstream file(argv[1]);
  if (!file) {
    cerr << "ERROR: cannot open file\n";
    return 1;
  }

  if (argc < 4) {
    cerr << "Usage: <INPUT> <T1> <T2>\n";
    return 1;
  }

  string word;
  vector<string> words;
  while (file >> word) {
    words.push_back(word);
  }

  cout << "Words: " << words.size() << "\n";

  // reference
  for (int i = 0; i < words.size(); i++) {
    auto &word = words[i];
    for (int j = 0; j < word.size(); j++) {
      auto &c = word[j];
      if (isLetter(c)) {
        letters_ref.append(c);
      }
    }
  }

  // parallel
  const int T = omp_get_max_threads();

  const int T1 = atoi(argv[2]);
  const int T2 = atoi(argv[3]);
  assert(T1 * T2 > 0);
  assert(T1 * T2 <= T);

  cout << "T1 = " << T1 << "\n";
  cout << "T2 = " << T2 << "\n";

  omp_set_max_active_levels(2);
  auto h0 = &letters;
  auto h1 = skynet::clause_split(T1, h0);

#pragma omp parallel num_threads(T1)
  {
#pragma omp for
    for (int i = 0; i < words.size(); i++) {
      auto &word = words[i];
      auto h2 = skynet::clause_split(T2, h1);

#pragma omp parallel num_threads(T2)
      {
#pragma omp for
        for (int j = 0; j < word.size(); j++) {
          auto &c = word[j];
          if (isLetter(c)) {
            h2->append(c);
          }
        }
      }
    }
  }

  letters.printInternals(/*separator=*/"");

  // correctness
  assert(letters.size() == letters_ref.size());
  for (int i = 0; i < letters.size(); i++) {
    assert(letters.at(i) == letters_ref.at(i));
  }

  return 0;
}
