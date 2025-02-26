#include <iostream>
#include <omp.h>
#include <vector>
#include <string>
#include <fstream>

#include "Sequence.hpp"
#include "Set.hpp"
#include "Skynet.hpp"
#include "HSequence.hpp"

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

  const int T1 = T / 2;
  const int T2 = T / T1;
  assert(T1 * T2 == T);

  cout << "T1 = " << T1 << "\n";
  cout << "T2 = " << T2 << "\n";

  omp_set_max_active_levels(2);
  letters.split(T1);
  // letters.__append(0, 'a');
  // letters.__append(1, 'b');
  // letters.__append(2, 'c');
  // letters.__append(3, 'd');
  // letters.printInternals("");
  //
  // letters.__split(0, 2);
  // letters.printInternals("");
  // return 0;
  // letters.__split(1, 2);
  // letters.printInternals("");

  // #pragma omp parallel for num_threads(T1)
  // for (int i = 0; i < T1; i++) {
  //   skynet::HSequence<char> *hs;
  //   hs = letters.findNthLeaf(omp_get_thread_num());
  //   hs->split(T2);
  // }
  // letters.printInternals();
  // return 0;

#pragma omp parallel num_threads(T1)
  {
    int t = omp_get_thread_num();
    auto hs = letters.findNthLeaf(t);
#pragma omp barrier
#pragma omp for
    for (int i = 0; i < words.size(); i++) {
      auto &word = words[i];
      hs->split(T2);
#pragma omp parallel num_threads(T2)
      {
        int tt = t * T2 + omp_get_thread_num();
#pragma omp for
        for (int j = 0; j < word.size(); j++) {
          auto &c = word[j];
          if (isLetter(c)) {
            // letters.__append(tt, c);
            hs->__append(omp_get_thread_num(), c);
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
