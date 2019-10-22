#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto found_it =  _lru_index.find(key);
    if (found_it == _lru_index.end()){
        return PutNew(key, value);
    }
    return UpdateEl(found_it, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if (_lru_index.find(key) != _lru_index.end()){
        return false;
    }
    return PutNew(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto found_it =  _lru_index.find(key);
    if (found_it == _lru_index.end()){
        return false;
    }
    return UpdateEl(found_it, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto found_it =  _lru_index.find(key);
    if (found_it == _lru_index.end()){
        return false;
    }
    std::unique_ptr<lru_node> temp;
    lru_node &del_node = found_it->second;
    _current_size -= del_node.key.size() + del_node.value.size();
    _lru_index.erase(found_it);

    if (del_node.next)
    {
        del_node.next->prev = del_node.prev;
    }
    if (del_node.prev)
    {
        temp.swap(del_node.prev->next);
        del_node.prev->next = std::move(del_node.next);
    }
    else
    {
        temp.swap(_lru_head);
        _lru_head = std::move(del_node.next);
    }

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto found_it =  _lru_index.find(key);
    if (found_it == _lru_index.end()){
        return false;
    }
    value = found_it->second.get().value;
    UpdatePos(found_it->second.get());
    return true;
}

bool SimpleLRU::PutNew(const std::string &key, const std::string &value){
    if (!GetSpace(key.size() + value.size())){
        return false;
    }
    std::unique_ptr<lru_node> put_node{new lru_node{key, value, nullptr, nullptr}};

    if (_lru_head != nullptr)
    {
        _lru_head->prev = put_node.get();
        put_node->next.swap(_lru_head);
        _lru_head.swap(put_node);
    }
    else
    {
        _lru_head.swap(put_node);
        _lru_tail = _lru_head.get();
    }

    _lru_index.insert(std::make_pair(std::reference_wrapper<const std::string>(_lru_head->key),
        std::reference_wrapper<lru_node>(*_lru_head)));
    _current_size += key.size() + value.size();
    return true;
}

bool SimpleLRU::UpdateEl(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
        std::less<std::string>>::iterator upt_it, const std::string &value){
    lru_node &edit_node = upt_it->second;
    if (!GetSpace(value.size() - edit_node.value.size()))
    {
        return false;
    }
    edit_node.value = value;
    UpdatePos(edit_node);
    return true;
}

void SimpleLRU::UpdatePos(lru_node &upt_node){
    if (&upt_node == _lru_head.get()){
        return;
    } else if (&upt_node != _lru_tail)
    {
        upt_node.next->prev = upt_node.prev;
    } else {
        _lru_tail = _lru_tail->prev;
    }

    std::unique_ptr<lru_node> temp = std::move(upt_node.prev->next);
    temp->prev->next.swap(temp->next);

    _lru_head->prev = temp.get();
    _lru_head.swap(temp->next);
    _lru_head.swap(temp);
    _lru_head->prev = nullptr;
}

bool SimpleLRU::GetSpace(std::size_t amount){
    if (amount > _max_size){
        return false;
    }
    while (_max_size - _current_size < amount){
       _current_size -= _lru_tail->key.size() + _lru_tail->value.size();
       _lru_index.erase(_lru_tail->key);

       if (_lru_tail->prev != nullptr){
           _lru_tail = _lru_tail->prev;
           _lru_tail->next.reset();
       } else {
           _lru_head.reset();
           _lru_tail = nullptr;
       }
    }

    return true;
}

} // namespace Backend
} // namespace Afina
