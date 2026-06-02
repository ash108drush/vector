#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <iterator>
#include <iostream>
template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }


    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept: buffer_(other.buffer_), capacity_(other.capacity_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    Vector() = default;
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    };
    iterator end() noexcept{
        return data_.GetAddress() + size_;
    };
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    };
    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    };
    const_iterator cbegin() const noexcept{
        return begin();
    }
    const_iterator cend() const noexcept{
        return end();
    };

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){

        size_t capacity = Capacity();
        const size_t index = pos - begin();

        if(index == size_){
            EmplaceBack(std::forward<Args>(args)...);
            return end() - 1;
        }

        if (size_ + 1 <= capacity) {
            T temp_data(std::forward<Args>(args)...);
            new (end()) T(std::move(*(end() - 1)));
            std::move_backward( begin() + index, end() - 1, end());
            *(begin() + index) = std::move(temp_data);
            ++size_;
            return data_.GetAddress() + index;
        }

        size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
        RawMemory<T> new_data(new_capacity);

        // Конструируем новый элемент
        new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);

        size_t constructed = 1; // новый элемент сконструирован

        try {
            // Копируем элементы до index
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), index,
                                          new_data.GetAddress());
            } else if constexpr (std::is_copy_constructible_v<T>) {
                std::uninitialized_copy_n(data_.GetAddress(), index,
                                          new_data.GetAddress());
            } else {
                std::uninitialized_move_n(data_.GetAddress(), index,
                                          new_data.GetAddress());
            }
            constructed += index;

            // Копируем элементы после index
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress() + index, size_ - index,
                                          new_data.GetAddress() + index + 1);
            } else if constexpr (std::is_copy_constructible_v<T>) {
                std::uninitialized_copy_n(data_.GetAddress() + index, size_ - index,
                                          new_data.GetAddress() + index + 1);
            } else {
                std::uninitialized_move_n(data_.GetAddress() + index, size_ - index,
                                          new_data.GetAddress() + index + 1);
            }
            constructed += (size_ - index);
        } catch(...) {
            // Откат: уничтожаем всё, что успели сконструировать
            std::destroy_n(new_data.GetAddress(), constructed);
            throw;
        }

        // Уничтожаем старые данные
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;

        return data_.GetAddress() + index;
  };

    iterator Erase(const_iterator pos){ /*noexcept(std::is_nothrow_move_assignable_v<T>)*/;
        size_t index = pos - cbegin();
        iterator mutable_pos = begin() + index;
        std::move(mutable_pos + 1, end(), mutable_pos);

        (end() - 1)->~T();

        size_--;
        return mutable_pos;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos,value);
    };
    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos,std::move(value));
    };

    explicit Vector(size_t size)
        : data_(size),
         size_(size)
    {

        size_t i = 0;
        try {
            for (; i != size; ++i) {
                new (data_ + i) T();
            }
        } catch (...) {
            // В переменной i содержится количество созданных элементов.
            // Теперь их надо разрушить
            DestroyN(data_.GetAddress(), i);

            throw;
        }
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        size_t i = 0;
        try {
            for (; i != other.size_; ++i) {
                CopyConstruct(data_.GetAddress() + i, other.data_[i]);
            }
        }catch (...) {
            DestroyN(data_.GetAddress(), i);

            throw;
        }
    }

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)) , size_(other.size_) {
        other.size_ = 0; // обнуляем размер у исходного объекта
    }

    size_t Capacity() const {
        return data_.Capacity();
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            // Уменьшение размера - always noexcept
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
            return;
        }

        if (new_size > size_) {
            if (new_size > Capacity()) {
                // Нужно выделить новую память
                RawMemory<T> new_data(new_size);

                // Сначала создаём новые элементы (может выбросить исключение)
                std::uninitialized_value_construct_n(new_data + size_, new_size - size_);

                // Затем перемещаем/копируем старые элементы
                if constexpr (std::is_nothrow_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
                    // Строгая гарантия
                    if constexpr (std::is_nothrow_move_constructible_v<T>) {
                        std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
                    } else {
                        // Есть копирующий конструктор, используем его для строгой гарантии
                        std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                    }
                } else {
                    // Базовая гарантия: move может бросать, копирования нет
                    std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
                }

                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            } else {
                // Ёмкости достаточно, просто создаём новые элементы на месте
                std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
            }
            size_ = new_size;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < Capacity()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        }

        size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
        RawMemory<T> new_data(new_capacity);
        new (new_data + size_) T(std::forward<Args>(args)...);  // Может выбросить, но это безопасно

        // Определяем стратегию перемещения старых элементов
        if constexpr (std::is_nothrow_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
            // Строгая гарантия: либо noexcept move, либо есть копирование
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                // Есть копирующий конструктор, используем его для строгой гарантии
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        } else {
            // Базовая гарантия: move может бросать, копирования нет
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
        return data_[size_ - 1];
    }

    void PushBack(const T& value) {
        if (size_ < Capacity()) {
            new (data_ + size_) T(value);
            ++size_;
            return;
        }

        size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
        RawMemory<T> new_data(new_capacity);
        new (new_data + size_) T(value);  // Может выбросить, но это безопасно

        // Определяем стратегию перемещения старых элементов
        if constexpr (std::is_nothrow_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
            // Строгая гарантия: либо noexcept move, либо есть копирование
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                // Есть копирующий конструктор, используем его для строгой гарантии
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        } else {
            // Базовая гарантия: move может бросать, копирования нет
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ < Capacity()) {
            if constexpr (std::is_move_constructible_v<T>) {
                new (data_ + size_) T(std::move(value));
            } else {
                new (data_ + size_) T(value);
            }
            ++size_;
            return;
        }

        size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
        RawMemory<T> new_data(new_capacity);

        // Конструируем новый элемент
        if constexpr (std::is_move_constructible_v<T>) {
            new (new_data + size_) T(std::move(value));  // Может выбросить
        } else {
            new (new_data + size_) T(value);  // Может выбросить
        }

        // Определяем стратегию перемещения старых элементов
        if constexpr (std::is_nothrow_move_constructible_v<T> || std::is_copy_constructible_v<T>) {
            // Строгая гарантия
            if constexpr (std::is_nothrow_move_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        } else {
            // Базовая гарантия: move может бросать, копирования нет
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
    }

    void PopBack() noexcept{
        if(size_ > 0){
            Destroy(data_.GetAddress() + size_ -1);
            --size_;
        }
    } ;


    size_t Size() const noexcept {
        return size_;
    }


    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);  // copy
                Swap(tmp);        // swap
            } else {
                auto copy_count = std::min(size_, rhs.size_);
                std::copy(rhs.data_.GetAddress(),
                          rhs.data_ + copy_count,
                          data_.GetAddress());  // копируем в существующие объекты

                if (size_ < rhs.size_) {
                    std::uninitialized_copy(rhs.data_ + copy_count,
                                            rhs.data_.GetAddress() + rhs.size_,
                                            data_.GetAddress() + size_);
                } else if (size_ > rhs.size_) {
                    std::destroy(data_.GetAddress() + copy_count, data_.GetAddress() + size_);  // уничтожаем лишние
                }

                size_ = rhs.size_;  // обновляем размер
            }
        }
        return *this;

    }
    Vector& operator=(Vector&& rhs) noexcept {
        data_.Swap(rhs.data_);
        std::swap(size_, rhs.size_);
        return *this;
    }

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_,other.size_);

    }

private:

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }


    RawMemory<T> data_;
    size_t size_ = 0;
};
