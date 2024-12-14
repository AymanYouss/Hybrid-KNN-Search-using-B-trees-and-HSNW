#include <iostream>
#include "bplustreecpp.h"

using namespace std;

int main(){
    BPlusTree bpt(2);
    bpt.insert(1);
    bpt.insert(2);
    bpt.insert(3);
    bpt.insert(4);
    bpt.insert(5);
    bpt.insert(6);
    

    

    bpt.printTree();


    return 0;
}
