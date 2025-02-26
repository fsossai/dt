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

int main(int argc, char *argv[]) {
  skynet::HSequence<char> letters;

  letters.split(4);

  letters.__append(0, 'a');
  letters.__append(0, 'b');
  letters.__append(0, 'c');
  letters.__append(1, 'd');
  letters.__append(1, 'e');
  letters.__append(1, 'f');

  letters.printInternals();
  cout << "splitLeaf()\n";

  letters.__append(2, 'g');
  letters.__append(2, 'h');
  letters.__append(2, 'i');

  letters.__append(3, 'j');
  letters.__append(3, 'k');

  letters.printInternals();
  cout << "\n";

  cout << "splitLeaf()\n";
  letters.findNthLeaf(2)->split(3);
  cout << "__append(4, x)\n";
  letters.__append(4, 'x');

  letters.printInternals();

  cout << "shrink()\n";
  letters.shrink();
  letters.printInternals();
  letters.print();

  return 0;
}
