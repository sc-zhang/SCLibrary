#include <string>
#include <vector>
#include <bitset>
#define MAX_SIZE (1<<30)


class BloomFilter{
private:
    std::bitset<MAX_SIZE>* bits;
    std::vector<int> hash_seed = {3, 5, 7, 11, 13, 31, 37, 61};
public:
    BloomFilter(){
        this->bits = new std::bitset<MAX_SIZE>;
    }
    void add(std::string key){
        for(int i=0; i<this->hash_seed.size(); i++){
            this->bits->set(this->hash(key, this->hash_seed[i]));
        }
    }
    bool contain(std::string key){
        for(int i=0; i<this->hash_seed.size(); i++){
            long long cur = this->hash(key, this->hash_seed[i]);
            if(!this->bits->test(cur)){
                return false;
            }
        }
        return true;
    }
    int hash(std::string key, int seed){
        long long res = 0;
        for(int i=0; i<key.size(); i++){
            res += res*seed+key[i];
        }
        return (MAX_SIZE-1)&res;
    }
    ~BloomFilter(){
        delete this->bits;
    }
};
