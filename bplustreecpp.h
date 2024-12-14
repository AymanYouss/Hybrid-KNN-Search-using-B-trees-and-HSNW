#include <iostream>
#include <vector>

using namespace std;
typedef long long ll;

struct Node {
    ll fanout;         // maximum capacity in this simplified code
    ll numberOfKeys;   // how many keys are actually in this node
    bool isLeaf;
    
    Node* parent;
    Node* next;        // singly linked list pointer for leaves
    
    vector<Node*> children;
    vector<ll> keys;

    Node(ll fanout, ll numberOfKeys, bool isLeaf) {
        this->fanout = fanout;        // 'fanout' here is analogous to "degree" in your code
        this->numberOfKeys = numberOfKeys;
        this->isLeaf = isLeaf;
        this->parent = NULL;
        this->next = NULL;

        // Allocate space in children and keys (fanout = max #keys for leaves, so max children is fanout+1)
        children.resize(fanout + 1, nullptr);
        keys.resize(fanout+1, 0LL);
    }
};

class BPlusTree {
    Node* root;
    ll degree;  // If you call this 'degree', treat it as "max #keys" in a node for simplicity

public:
    BPlusTree(ll degree) {
        this->degree = degree;
        root = new Node(degree, 0, true);
        cout << "Root created\n";
    }

    void insert(ll value) {
        cout << "Inserting " << value << endl;
        Node* current = root;

        // 1. Traverse down to a leaf where 'value' should be inserted
        while (!current->isLeaf) {
            ll i = 0;
            while (i < current->numberOfKeys && current->keys[i] < value) {
                i++;
            }
            current = current->children[i];
        }

        // 2. Insert 'value' into this leaf in sorted order
        insertIntoLeaf(current, value);

        // 3. If leaf is over capacity, split
        if (current->numberOfKeys > degree) {
            splitLeaf(current);
        }
    }

private:
    // Utility to insert 'value' into a leaf node in sorted order
    void insertIntoLeaf(Node* leaf, ll value) {
        ll i = leaf->numberOfKeys - 1;
        // Shift keys right until we find the correct position for 'value'
        while (i >= 0 && i < leaf->fanout && leaf->keys[i] > value) {
            leaf->keys[i + 1] = leaf->keys[i];
            i--;
        }
        // Insert the new key
        leaf->keys[i + 1] = value;
        leaf->numberOfKeys++;
    }

    // Split a *leaf* node that has overflowed
    void splitLeaf(Node* leaf) {
        cout << "Splitting leaf\n";
        Node* newLeaf = new Node(degree, 0, true);
        
        // Temporary buffer to hold all keys (old + new)
        vector<ll> tempKeys(leaf->numberOfKeys);
        for (ll i = 0; i < leaf->numberOfKeys; i++) {
            tempKeys[i] = leaf->keys[i];
        }

        // We'll split roughly in half
        ll splitIndex = (leaf->numberOfKeys) / 2;  
        // For B+‐tree leaf nodes, some implementations might use “ceil((fanout+1)/2)” etc.
        // This code takes the lower half in 'leaf', upper half in 'newLeaf'

        leaf->numberOfKeys = splitIndex;
        newLeaf->numberOfKeys = tempKeys.size() - splitIndex;
        
        // Put first half back in 'leaf'
        for (ll i = 0; i < splitIndex; i++) {
            leaf->keys[i] = tempKeys[i];
        }
        // Put the second half in 'newLeaf'
        for (ll i = 0; i < newLeaf->numberOfKeys; i++) {
            newLeaf->keys[i] = tempKeys[splitIndex + i];
        }

        // Link the leaves
        newLeaf->next = leaf->next;
        leaf->next = newLeaf;

        // Set parents
        newLeaf->parent = leaf->parent;

        // The "separator key" that goes up to parent is the first key of newLeaf
        ll separatorKey = newLeaf->keys[0];

        // If leaf was root (no parent), create a new root
        if (leaf->parent == NULL) {
            Node* newRoot = new Node(degree, 1, false);
            newRoot->keys[0] = separatorKey;
            newRoot->children[0] = leaf;
            newRoot->children[1] = newLeaf;
            leaf->parent = newRoot;
            newLeaf->parent = newRoot;
            root = newRoot;
            cout << "New root created\n";
        } else {
            // Insert the separatorKey and pointer to newLeaf into the parent
            insertInParent(leaf->parent, separatorKey, newLeaf);
        }
    }

    // Insert (value, newChildPtr) into parent node 'parent'
    // If parent overflows, split the parent as well (recursively)
    void insertInParent(Node* parent, ll value, Node* newChild) {
        cout << "Inserting in parent\n";
        // Insert 'value' into 'parent' in sorted order
        ll i = parent->numberOfKeys - 1;
        // Shift keys to the right to find correct position
        while (i >= 0 && i < parent->fanout && parent->keys[i] > value) {
            parent->keys[i + 1] = parent->keys[i];
            parent->children[i + 2] = parent->children[i + 1];
            i--;
        }
        parent->keys[i + 1] = value;
        parent->children[i + 2] = newChild;
        parent->numberOfKeys++;
        newChild->parent = parent;

        // Check for overflow
        if (parent->numberOfKeys > degree) {
            splitInternal(parent);
        }
    }

    // Split an *internal* node that has overflowed
    void splitInternal(Node* internal) {
        cout << "Splitting internal node\n";
        Node* newInternal = new Node(degree, 0, false);

        // Temp arrays (internal->numberOfKeys keys, internal->numberOfKeys+1 children)
        vector<ll> tempKeys(internal->numberOfKeys);
        vector<Node*> tempChildren(internal->numberOfKeys + 1);

        for (ll i = 0; i < internal->numberOfKeys; i++) {
            tempKeys[i] = internal->keys[i];
        }
        for (ll i = 0; i <= internal->numberOfKeys; i++) {
            tempChildren[i] = internal->children[i];
        }

        ll totalKeys = internal->numberOfKeys;
        ll splitIndex = totalKeys / 2;  // median for internal node

        // The "separator" that moves up to the parent is tempKeys[splitIndex]
        ll separatorKey = tempKeys[splitIndex];

        // Left side keeps [0..splitIndex-1] keys
        internal->numberOfKeys = splitIndex;
        for (ll i = 0; i < splitIndex; i++) {
            internal->keys[i] = tempKeys[i];
        }
        // Left side children are [0..splitIndex]
        for (ll i = 0; i <= splitIndex; i++) {
            internal->children[i] = tempChildren[i];
        }

        // Right side (newInternal) has [splitIndex+1..end] keys
        newInternal->numberOfKeys = totalKeys - (splitIndex + 1);
        for (ll i = 0; i < newInternal->numberOfKeys; i++) {
            newInternal->keys[i] = tempKeys[splitIndex + 1 + i];
        }
        // Right side children are [splitIndex+1..end]
        for (ll i = 0; i <= newInternal->numberOfKeys; i++) {
            newInternal->children[i] = tempChildren[splitIndex + 1 + i];
            if (newInternal->children[i]) {
                newInternal->children[i]->parent = newInternal;
            }
        }

        newInternal->parent = internal->parent;

        // If 'internal' was root, create a new root
        if (internal->parent == NULL) {
            Node* newRoot = new Node(degree, 1, false);
            newRoot->keys[0] = separatorKey;
            newRoot->children[0] = internal;
            newRoot->children[1] = newInternal;
            internal->parent = newRoot;
            newInternal->parent = newRoot;
            root = newRoot;
            cout << "Created new root (internal split)\n";
        } else {
            // Insert separatorKey and pointer to newInternal in the parent
            insertInParent(internal->parent, separatorKey, newInternal);
        }
    }

public:
    void printTree() {
        printNode(root, 0);
    }

private:
    void printNode(Node* node, int level) {
        if (!node) return;

        // Indent to visualize tree structure
        for (int i = 0; i < level; i++) cout << "    ";

        cout << (node->isLeaf ? "[Leaf] " : "[Int ] ");
        cout << "Keys(" << node->numberOfKeys << "): ";
        for (ll i = 0; i < node->numberOfKeys; i++) {
            cout << node->keys[i] << " ";
        }
        cout << "\n";

        // Recursively print children of an internal node
        if (!node->isLeaf) {
            for (ll i = 0; i <= node->numberOfKeys; i++) {
                printNode(node->children[i], level + 1);
            }
        }
    }
};