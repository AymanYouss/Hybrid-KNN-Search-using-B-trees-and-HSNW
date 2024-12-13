#include <iostream>
#include <vector>

using namespace std;
typedef long long ll;


struct Node{
    ll fanout;
    ll numberOfKeys;

    bool isLeaf;
    ll data;
    Node* parent;
    Node* next;

    vector<Node*> children;
    vector<ll> keys;

public:
    Node(ll fanout, ll numberOfKeys, bool isLeaf){
        this->fanout = fanout;
        this->numberOfKeys = numberOfKeys;
        this->isLeaf = isLeaf;
        this->parent = NULL;

        // Children and items that should be inserted before
        for(ll i = 0; i < fanout+1; i++){
            children.push_back(NULL);
        }

        for(ll i = 0; i < fanout; i++){
            keys.push_back(0);
        }

    }
    
};

class BPlusTree{
    Node* root;
    ll degree;

public:
    BPlusTree(ll degree){
        this->degree = degree;
        root = new Node(degree, 0, true);
        cout << "Root created" << endl;
    }

    void insert(ll value){
        Node *current = this->root;
        cout << "Inserting " << value << endl;
        

        while(!current->isLeaf){
            ll i = 0;
            cout << "Current node is not a leaf" << endl;
            while(i < current->numberOfKeys && current->keys[i] < value){
                i++;
            }
            cout << "i: " << i << endl;
            current = current->children[i];
        }

        //current is now a leaf in the node where we should insert the data
        if (current->numberOfKeys < degree){
            current->keys[current->numberOfKeys] = value;
            cout << "Inserting in leaf" << endl;
            current->numberOfKeys++;
        } else{
            // Split the node
            cout << "Splitting node" << endl;
            Node* newLeaf = new Node(degree, 0, true);
            vector<ll> temp(this->degree+1);
            for(ll i = 0; i < this->degree; i++){
                temp[i] = current->keys[i];
            }

            temp[this->degree] = value;
            if (this->degree % 2 == 0){
                current->numberOfKeys = this->degree/2;
                newLeaf->numberOfKeys = this->degree/2;
            } else{
                current->numberOfKeys = this->degree/2;
                newLeaf->numberOfKeys = this->degree/2 + 1;
            }

            for(ll i = 0; i < current->numberOfKeys; i++){
                current->keys[i] = temp[i];
            }
            for(ll i = 0; i < newLeaf->numberOfKeys; i++){
                newLeaf->keys[i] = temp[i+current->numberOfKeys];
            }

            newLeaf->next = current->next;
            current->next = newLeaf;

            if (current->parent == NULL){
                Node* newRoot = new Node(degree, 1, false);
                newRoot->keys[0] = newLeaf->keys[0];
                newRoot->children[0] = current;
                newRoot->children[1] = newLeaf;
                current->parent = newRoot;
                newLeaf->parent = newRoot;
                this->root = newRoot;
            } else{
                Node* parent = current->parent;
               //check if full
               if(parent->numberOfKeys < degree){
                   ll i = 0;
                   while(i < parent->numberOfKeys && parent->keys[i] < newLeaf->keys[0]){
                       i++;
                   }

                   for(ll j = parent->numberOfKeys; j > i; j--){
                       parent->keys[j] = parent->keys[j-1];
                   }

                   for(ll j = parent->numberOfKeys+1; j > i+1; j--){
                       parent->children[j] = parent->children[j-1];
                   }

                   parent->keys[i] = newLeaf->keys[0];
                   parent->children[i+1] = newLeaf;
                   parent->numberOfKeys++;
                   newLeaf->parent = parent;
               } else{
                   // Split the parent
                   Node* newParent = new Node(degree, 0, false);
                   vector<ll> tempKeys(this->degree+1);
                   vector<Node*> tempChildren(this->degree+2);
                   for(ll i = 0; i < parent->numberOfKeys; i++){
                       tempKeys[i] = parent->keys[i];
                   }
                   for(ll i = 0; i < parent->numberOfKeys+1; i++){
                       tempChildren[i] = parent->children[i];
                   }

                   tempKeys[parent->numberOfKeys] = newLeaf->keys[0];
                   tempChildren[parent->numberOfKeys+1] = newLeaf;

                   if (this->degree % 2 == 0){
                       parent->numberOfKeys = this->degree/2;
                       newParent->numberOfKeys = this->degree/2;
                   } else{
                       parent->numberOfKeys = this->degree/2;
                       newParent->numberOfKeys = this->degree/2 + 1;
                   }

                   for(ll i = 0; i < parent->numberOfKeys; i++){
                       parent->keys[i] = tempKeys[i];
                   }
                   for(ll i = 0; i < newParent->numberOfKeys; i++){
                       newParent->keys[i] = tempKeys[i+parent->numberOfKeys+1];
                   }

                   for(ll i = 0; i < parent->numberOfKeys+1; i++){
                       parent->children[i] = tempChildren[i];
                   }
                   for(ll i = 0; i < newParent->numberOfKeys+1; i++){
                       newParent->children[i] = tempChildren[i+parent->numberOfKeys];
                   }

            }


        }
            
        
    }
    }

    void printTree(){
        print(root);
    }

    void print(Node* node){
        if(node == NULL){
            return;
        }

        for(ll i = 0; i < node->numberOfKeys; i++){
            cout << node->keys[i] << " ";
        }

        cout << endl;

        for(ll i = 0; i < node->fanout; i++){
            print(node->children[i]);
        }
    }

    

};