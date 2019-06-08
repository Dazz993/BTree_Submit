// definition ok
// it's ok
// with file operation ok
// with find_leaf ok
// with insert_leaf ok
// with insert_inNode ok
// with split_leaf ok
// with split_inNode ok
// with specific BPT operation ok
// of course only insert
// with insert ok
// 13:33
// f*** 3 hours for repeadly opening and closing
#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <fstream>

namespace sjtu {

    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    private:
        typedef size_t OFFSET_TYPE;
        typedef size_t NODE_TYPE;
        typedef size_t SIZE_TYPE;
        const int blockSize = 4096;
        static const int M = 258;

        struct NameStr {
            char *str;

            NameStr() {
                str = new char[8];
                str[0] = 'b';
                str[1] = 'p';
                str[2] = 't';
                str[3] = '.';
                str[4] = 'd';
                str[5] = 'a';
                str[6] = 't';
                str[7] = '\0';
            }

            ~NameStr() {
                delete str;
            }
        };

        struct BasicInfo {
            NODE_TYPE head;
            NODE_TYPE tail;
            NODE_TYPE root;
            size_t sz;
            OFFSET_TYPE eof;

            BasicInfo() : head(0), tail(0), root(0), sz(0), eof(0) {}
        };

        struct inNode {
            OFFSET_TYPE offset;        // offset
            NODE_TYPE par;            // parent
            //OFFSET_TYPE leaves[MMIN + 1];
            std::pair<Key, OFFSET_TYPE> index[M + 1]; //first is key, second is offset of the ch(it can be inNode or leaf)
            int cnt;                // the size of index
            bool type;                // if type == 1, child is leaf, else child is inNode
            inNode() : offset(0), par(0), cnt(0), type(0) {
                for (int i = 0; i <= M; ++i) {
                    index[i].second = 0;
                }
            }
        };

        struct leafNode {
            OFFSET_TYPE offset;          // offset
            NODE_TYPE par;               // parent
            NODE_TYPE pre, nex;          // pre and nex leaf
            int cnt;                  //the size of data
            std::pair<Key, Value> data[M + 1];   // data
            leafNode() : offset(0), par(0), pre(0), nex(0), cnt(0) {
                for (int i = 0; i <= M; i++) {
                    data[i].first = 0;
                    data[i].second = 0;
                }
            }
        };

    public:
        class iterator;
        class const_iterator;

    private:
        FILE *fp;
        NameStr fp_name;
        BasicInfo info;
        bool fp_open; // if fp_open = 1, it means the file is open; else it's close

//==============================file operation===============================//
        bool file_exists;

        /*
         * function: if the file is not open, open it
         * it's important to update the file_exists; or every open will make the file empty;
         */
        void open_file() {
            file_exists = 1;
            if (fp_open == 0) {
                fp = fopen(fp_name.str, "rb+");
                if (fp == NULL) {
                    file_exists = 0;
                    fp = fopen(fp_name.str, "wb+");
                    fclose(fp);
                    fp = fopen(fp_name.str, "rb+");
//                    if(fp == NULL){
//                        std::cout<<"open_file error()__signal(1)\n";
//                    }
                } else read_file(&info, 0, sizeof(BasicInfo), 1);
            }
            fp_open = 1;
        }

        /*
         * function: close the file
         */
        void close_file() {
            if (fp_open == 1) {
                fclose(fp);
                fp_open = 0;
            }
        }

        /*
         * function: from the offset, read sz * count bytes to the place;
         */
        void read_file(void *place, OFFSET_TYPE offset, SIZE_TYPE sz, int count) {
            if (fseek(fp, offset, SEEK_SET)) {
//                std::cout<<"write_file error()__signal(1)  open file failed!"<<std::endl;
                throw "open file failed";
            }
            fread(place, sz, count, fp);
        }

        /*
         * function: from the place, write sz * count bytes to offset;
         */
        void write_file(void *place, OFFSET_TYPE offset, SIZE_TYPE sz, int count) {
            if (fseek(fp, offset, SEEK_SET)) {
//                std::cout<<"write_file error()__signal(1)  open file failed!"<<std::endl;
                throw "open file failed";
            }
            fwrite(place, sz, count, fp);
        }
//==============================file operations end=============================//
//=============================================================================//
//==============================BTree operations===============================//
    public:
        /*
         * function: build_empty_tree
         * only when file_exists == 0, call it
         */
        void build_empty_tree() {
            leafNode _leaf;
            inNode _root;

            //adjust info
            info.head = sizeof(BasicInfo) + sizeof(inNode);
            info.tail = sizeof(BasicInfo) + sizeof(inNode);
            info.sz = 0;
            info.root = sizeof(BasicInfo);
            info.eof = sizeof(BasicInfo) + sizeof(inNode) + sizeof(leafNode);

            //adjust root
            _root.offset = sizeof(BasicInfo);
            _root.par = 0;
            _root.cnt = 1; // no key
            _root.type = 1; // its ch is leafNode
            _root.index[0].second = sizeof(BasicInfo) + sizeof(inNode);

            //adjust root->leaf
            _leaf.offset = sizeof(BasicInfo) + sizeof(inNode);
            _leaf.par = _root.offset;
            _leaf.pre = _leaf.nex = 0;
            _leaf.cnt = 0;

            //put them into file
            write_file(&info, 0, sizeof(BasicInfo), 1);
            write_file(&_root, _root.offset, sizeof(inNode), 1);
            write_file(&_leaf, _leaf.offset, sizeof(leafNode), 1);
        }

        /*
         *function: given a key, find the belonging leaf
         * return the offset of the leaf
         */
        OFFSET_TYPE find_leaf(const Key &key, OFFSET_TYPE offset) {
            inNode _node;
            read_file(&_node, offset, sizeof(inNode), 1);
            int pos = 0;
            for (; pos < _node.cnt; ++pos)
                if (key < _node.index[pos].first) break;
            if (pos == 0) return 0; //what you want is impossibly in this node for its smaller than the first key

            //its ch is leafNode, and the offset is the pos of the leaf
            if (_node.type == 1) return _node.index[pos - 1].second;
                //its ch is inNode, you should find it continuously
            else return find_leaf(key, _node.index[pos - 1].second);
        }

        /*
         * function: insert an element to the leaf it should be in.
         * if after insertion, size of leaf is bigger than M , split it.
         */
        std::pair<iterator, OperationResult> insert_leaf(leafNode &_leaf, const Key &_key, const Value &_value) {
//				std::cout<<"// begin insert_leaf"<<std::endl;
//				std::cout<<"// during insert_leaf: _leaf.cnt = "<<_leaf.cnt<<std::endl;
            iterator ret;
            int pos = 0;
            //find the pos it should insert
            for (; pos < _leaf.cnt; pos++) {
                if (_key == _leaf.data[pos].first) {
                    //there is the same element in the bpt
//                    std::cout<<"insert_leaf failed()__signal(1)  there is the same key in the bpt!";
                    return std::pair<iterator, OperationResult>(iterator(NULL), Fail);
                }
                if (_key < _leaf.data[pos].first) break;
            }
            //insert the data
            for (int i = _leaf.cnt - 1; i >= pos; i--) {
                _leaf.data[i + 1] = _leaf.data[i];
            }
            _leaf.data[pos].first = _key;
            _leaf.data[pos].second = _value;
            _leaf.cnt++;
            info.sz++;

            ret.belong = this;
            ret.place = pos;
            ret.offset = _leaf.offset;

            write_file(&info, 0, sizeof(BasicInfo), 1);
            if (_leaf.cnt <= M) { //do not need to split
                write_file(&_leaf, _leaf.offset, sizeof(leafNode), 1);
            } else { //need to split
                split_leaf(_leaf, ret, _key);
            }
            return std::pair<iterator, OperationResult>(ret, Success);
        };

        /*
         * function: If the size of leaf is longer than M, split it into two leaf nodes
         * call insert_inNode() to insert a (key, ch) std::pair in the par node
         */
        void split_leaf(leafNode &_leaf, iterator &itr, const Key _key) {
            leafNode _newleaf;

            //adjust newleaf
            _newleaf.cnt = _leaf.cnt - _leaf.cnt / 2;
            _newleaf.par = _leaf.par;
            _newleaf.offset = info.eof;
            _newleaf.nex = _leaf.nex;
            _newleaf.pre = _leaf.offset;
            for (int pos = 0; pos < _newleaf.cnt; pos++) {
                _newleaf.data[pos] = _leaf.data[pos + _leaf.cnt / 2];
                if (_newleaf.data[pos].first == _key) {
                    itr.offset = _newleaf.offset;
                    itr.place = pos;
                }
            }
            if (_newleaf.nex != 0) { //if it has next leaf, adjust next leaf's pre
                leafNode _temp;
                read_file(&_temp, _newleaf.nex, sizeof(leafNode), 1);
                _temp.pre = _newleaf.offset;
                write_file(&_temp, _temp.offset, sizeof(leafNode), 1);
            } else { //if it is end, adjust the info.tail
                info.tail = _newleaf.offset;
            }

            //adjust leaf
            _leaf.cnt /= 2;
            _leaf.nex = _newleaf.offset;

            info.eof += sizeof(leafNode);

            write_file(&_leaf, _leaf.offset, sizeof(leafNode), 1);
            write_file(&_newleaf, _newleaf.offset, sizeof(leafNode), 1);
            write_file(&info, 0, sizeof(BasicInfo), 1);

            //adjust its par
            inNode _par;
            read_file(&_par, _leaf.par, sizeof(inNode), 1);
            insert_inNode(_par, _newleaf.data[0].first, _newleaf.offset);
        }

        /*
         * function: insert an key to the given inNode.
         *           insert an child to the given inNode.
         * if the size of inNode is bigger than M , spilt it.
         */
        void insert_inNode(inNode &_node, const Key &key, OFFSET_TYPE offsetOfch) {
            //it's easy
            int pos = 0;
            for (; pos < _node.cnt; pos++)
                if (key < _node.index[pos].first) break;
            for (int i = _node.cnt - 1; i >= pos; i--) {
                _node.index[i + 1] = _node.index[i];
            }
            _node.index[pos].first = key;
            _node.index[pos].second = offsetOfch;
            _node.cnt++;
            if (_node.cnt <= M) write_file(&_node, _node.offset, sizeof(inNode), 1);
            else split_inNode(_node);
        }

        /*
         * function: split a inNode into two parts
         */
        void split_inNode(inNode &_node) {
//            std::cout<<"split_inNode()__signal(1)\n";

            inNode _newnode;
            //adjust newnode
            _newnode.cnt = _node.cnt - _node.cnt / 2;
            _newnode.par = _node.par;
            _newnode.offset = info.eof;
            _newnode.type = _node.type;
            for (int pos = 0; pos < _newnode.cnt; pos++) {
                _newnode.index[pos] = _node.index[pos + _node.cnt / 2];
            }
//
            //adjust its sons, let their par -> new node
            leafNode _templeaf;
            inNode _tempinNode;
            for (int i = 0; i < _newnode.cnt; i++) {
                if (_newnode.type == 1) { //its ch is leaf node
                    read_file(&_templeaf, _newnode.index[i].second, sizeof(leafNode), 1);
                    _templeaf.par = _newnode.offset;
                    write_file(&_templeaf, _templeaf.offset, sizeof(leafNode), 1);
                } else { //its ch is inNode
                    read_file(&_tempinNode, _newnode.index[i].second, sizeof(inNode), 1);
                    _tempinNode.par = _newnode.offset;
                    write_file(&_templeaf, _tempinNode.offset, sizeof(inNode), 1);
                }
            }

            _node.cnt /= 2;

            info.eof += sizeof(inNode);

            // the situation it's root
            if (_node.offset == info.root) { // the situation it's root
                inNode _newroot;
                _newroot.offset = info.eof;
                info.eof += sizeof(inNode);
                _newroot.cnt = 2;
                _newroot.index[0].first = _node.index[0].first;
                _newroot.index[0].second = _node.offset;
                _newroot.index[1].first = _newnode.index[0].first;
                _newroot.index[1].second = _newnode.offset;
                info.root = _node.par = _newnode.par = _newroot.offset;

                write_file(&info, 0, sizeof(BasicInfo), 1);
                write_file(&_node, _node.offset, sizeof(inNode), 1);
                write_file(&_newnode, _newnode.offset, sizeof(inNode), 1);
                write_file(&_newroot, _newroot.offset, sizeof(inNode), 1);
            } else {
                write_file(&info, 0, sizeof(BasicInfo), 1);
                write_file(&_node, _node.offset, sizeof(inNode), 1);
                write_file(&_newnode, _newnode.offset, sizeof(inNode), 1);

                inNode par;
                read_file(&par, _node.par, sizeof(inNode), 1);
                insert_inNode(par, _newnode.index[0].first, _newnode.offset);
            }
        }
//==============================BTree operations end=============================//
    public:
        class iterator {
            friend class BTree;

        private:
            OFFSET_TYPE offset;        // offset of the leaf node
            int place;                            // place of the element in the leaf node
            BTree *belong;

        public:
//        bool modify(const Value& value){
//121
//        }

            iterator() : offset(0), belong(NULL), place(0) {}

            iterator(BTree *_belong, OFFSET_TYPE _offset = 0, int _place = 0) {
                belong = _belong;
                offset = _offset;
                place = _place;
            }

            iterator(const iterator &other) {
                belong = other.belong;
                offset = other.offset;
                place = other.place;
            }

            // Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                // Todo iterator++
            }
            iterator& operator++() {
                // Todo ++iterator
            }
            iterator operator--(int) {
                // Todo iterator--
            }
            iterator& operator--() {
                // Todo --iterator
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                // Todo operator ==
            }
            bool operator==(const const_iterator& rhs) const {
                // Todo operator ==
            }
            bool operator!=(const iterator& rhs) const {
                // Todo operator !=
            }
            bool operator!=(const const_iterator& rhs) const {
                // Todo operator !=
            }
        };

        class const_iterator {
            // it should has similar member method as iterator.
            //  and it should be able to construct from an iterator.
            friend class BTree;

        private:
            // Your private members go here
            OFFSET_TYPE offset;
            BTree *belong;
            int place;

        public:
            const_iterator() : offset(0), belong(NULL), place(0) {}

            const_iterator(BTree *_belong, OFFSET_TYPE _offset = 0, int _place = 0) {
                belong = _belong;
                offset = _offset;
                place = _place;
            }

            const_iterator(const const_iterator &other) {
                belong = other.belong;
                offset = other.offset;
                place = other.place;
            }

            const_iterator(const iterator &other) {
                belong = other.belong;
                offset = other.offset;
                place = other.place;
            }

            iterator operator++(int) {
                // Todo iterator++
            }
            iterator& operator++() {
                // Todo ++iterator
            }
            iterator operator--(int) {
                // Todo iterator--
            }
            iterator& operator--() {
                // Todo --iterator
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                // Todo operator ==
            }
            bool operator==(const const_iterator& rhs) const {
                // Todo operator ==
            }
            bool operator!=(const iterator& rhs) const {
                // Todo operator !=
            }
            bool operator!=(const const_iterator& rhs) const {
                // Todo operator !=
            }
        };
//

//
        // Default Constructor and Copy Constructor
        BTree() {
            fp = nullptr;
            open_file();
            if (file_exists == 0)build_empty_tree();
        }

        BTree(const BTree &other) {
            // Todo Copy
        }

        BTree &operator=(const BTree &other) {
            // Todo Assignment
        }

        ~BTree() {
            close_file();
        }

        // Insert: Insert certain Key-Value into the database
        // Return a std::pair, the first of the std::pair is the iterator point to the new
        // element, the second of the std::pair is Success if it is successfully inserted
        std::pair<iterator, OperationResult> insert(const Key &key, const Value &value) {
//            std::cout<<"// begin to insert" <<std::endl;
            // to find the key it should be in
            OFFSET_TYPE offsetOfleaf = find_leaf(key, info.root);
//            std::cout<<"// during insert: leaf_offset it should be in:"<<offsetOfleaf<<std::endl;
            leafNode _leaf;
            if (info.sz == 0 || offsetOfleaf == 0) {
                //offsetOfleaf == 0 means it's the smallest elements, it should be in the newed head leaf
//                std::cout<<"// during insert: it's the smallest element"<<std::endl;
                read_file(&_leaf, info.head, sizeof(leafNode), 1);
                std::pair<iterator, OperationResult> ret = insert_leaf(_leaf, key, value);
                if (ret.second == Fail) {
                    //if there is the same element in the leaves , it will Fail;
//                    std::cout<<"insert error()__signal(1)  std::pair ret.second == Fail"<<std::endl;
                    return ret;
                }
                //adjust parents
                OFFSET_TYPE _offset = _leaf.par;
                inNode _par;
                while (_offset != 0) {
                    read_file(&_par, _offset, sizeof(inNode), 1);
                    _par.index[0].first = key;
                    write_file(&_par, _offset, sizeof(inNode), 1);
                    _offset = _par.par;
                }
                return ret;
            }
            read_file(&_leaf, offsetOfleaf, 1, sizeof(leafNode));
//            std::cout<<"// during insert: _leaf.sz = "<<_leaf.cnt<<std::endl;
            std::pair<iterator, OperationResult> ret = insert_leaf(_leaf, key, value);
            return ret;
        }

        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        OperationResult erase(const Key &key) {
            // TODO erase function
            return Fail;  // If you can't finish erase part, just remaining here.
        }

        // Return a iterator to the beginning
        iterator begin() {
            return iterator(this, info.head, 0);
        }

        const_iterator cbegin() const {
            return const_iterator(this, info.head, 0);
        }

        // Return a iterator to the end(the next element after the last)
        iterator end() {
            leafNode _tail;
            read_file(&_tail, info.tail, 1, sizeof(leafNode));
            return iterator(this, info.tail, _tail.cnt);
        }

        const_iterator cend() const {
            leafNode _tail;
            read_file(&_tail, info.tail, 1, sizeof(leafNode));
            return const_iterator(this, info.tail, _tail.cnt);
        }

        // Check whether this BTree is empty
        bool empty() const {
            return info.sz == 0;
        }

        // Return the number of <K,V> pairs
        size_t size() const {
            return info.sz;
        }

        // Clear the BTree
        void clear() {
            fp = fopen(fp_name.str, "w");
            fclose(fp);
            open_file();
            build_empty_tree();
        }

        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key &key) const {
            size_t flag = 0;
            if (find(key) != iterator(NULL)) flag = 1;
            return flag;
        }

        // Return the value refer to the Key(key)
        Value at(const Key &key) {
            iterator itr = find(key);
            leafNode _leaf;
            if (itr == end()) {
                throw "not found";
            }
            read_file(&_leaf, itr.offset, sizeof(leafNode), 1);
            return _leaf.data[itr.place].second;
        }

        /**
         * Finds an element with key equivalent to key.
         * key value of the element to search for.
         * Iterator to an element with key equivalent to key.
         *   If no such element is found, past-the-end (see end()) iterator is
         * returned.`
         */
        iterator find(const Key &key) {
            OFFSET_TYPE leaf_offset = find_leaf(key, info.root);
            if (leaf_offset == 0) return end(); // if return end, it means not being found
            leafNode leaf;
            read_file(&leaf, leaf_offset, 1, sizeof(leafNode));
            for (int i = 0; i < leaf.cnt; ++i)
                if (leaf.data[i].first == key) return iterator(this, leaf_offset, i);
            return end();
        }

        const_iterator find(const Key &key) const {
            OFFSET_TYPE offsetOfleaf = find_leaf(key);
            if (offsetOfleaf == 0) return cend(); // if return cend, it means not being found
            leafNode leaf;
            read_file(&leaf, offsetOfleaf, sizeof(leafNode), 1);
            for (int i = 0; i < leaf.cnt; ++i)
                if (leaf.data[i].first == key) return const_iterator(this, offsetOfleaf, i);
            return cend();
        }
    };
}  // namespace sjtu
