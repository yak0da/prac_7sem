#pragma once

#include <atomic>
#include <compare>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace smart {

// Внутренние детали реализации, снаружи напрямую не используются.
namespace detail {

// Управляющий блок: общий для всех копий SharedPtr, указывающих на один объект.
// Хранит счетчик владельцев и умеет уничтожить объект, когда владельцев не осталось.
struct ControlBlockBase {
    // Число SharedPtr, владеющих объектом. Атомарный, т.к. копии могут
    // создаваться и уничтожаться одновременно из разных потоков.
    std::atomic<long> strong{1};

    // Виртуальный деструктор: блок удаляется через указатель на базовый класс.
    virtual ~ControlBlockBase() = default;

    // Уничтожает управляемый объект. Как именно - зависит от вида блока (см. наследников).
    virtual void destroyObject() noexcept = 0;

    // Появился еще один владелец: увеличиваем счетчик.
    // relaxed достаточно - новый владелец появляется из уже существующего.
    void addRef() noexcept { strong.fetch_add(1, std::memory_order_relaxed); }

    // Один владелец ушел: уменьшаем счетчик. Если он был последним -
    // уничтожаем объект и сам управляющий блок.
    // acq_rel гарантирует, что все изменения объекта из других потоков
    // видны к моменту вызова его деструктора.
    void release() noexcept {
        if (strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroyObject();
            delete this;
        }
    }
};

// Блок для объекта, созданного отдельно через new (SharedPtr<T> p(new T)).
// Запоминает исходный тип U, поэтому SharedPtr<Base>, полученный из
// SharedPtr<Derived>, удалит объект именно как Derived.
template <class U>
struct PointerControlBlock final : ControlBlockBase {
    U* ptr;

    // Запоминает указатель на объект, которым будет владеть.
    explicit PointerControlBlock(U* p) noexcept : ptr(p) {}

    // Удаляет объект через delete с исходным типом.
    void destroyObject() noexcept override { delete ptr; }
};

// Блок, внутри которого лежит сам объект: объект и счетчик выделяются
// одним new (используется в makeShared, экономит одно выделение памяти).
template <class U>
struct InplaceControlBlock final : ControlBlockBase {
    // Сырая память нужного размера и выравнивания под объект U.
    alignas(U) unsigned char storage[sizeof(U)];

    // Конструирует объект U прямо в storage (placement new), передавая аргументы.
    template <class... Args>
    explicit InplaceControlBlock(Args&&... args) {
        ::new (static_cast<void*>(storage)) U(std::forward<Args>(args)...);
    }

    // Указатель на объект внутри storage.
    U* get() noexcept { return std::launder(reinterpret_cast<U*>(storage)); }

    // Вызывает только деструктор объекта: память освободится вместе с блоком.
    void destroyObject() noexcept override { get()->~U(); }
};

}  // namespace detail






// Умный указатель с разделяемым владением (аналог std::shared_ptr).
// Несколько SharedPtr могут указывать на один объект; объект удаляется,
// когда уничтожается (или сбрасывается) последний из них.
template <class T>
class SharedPtr {
public:
    using element_type = T;

    // Конструктор по умолчанию: пустой указатель, ничем не владеет.
    constexpr SharedPtr() noexcept = default;

    // Конструктор от nullptr: тоже пустой указатель (позволяет писать SharedPtr<T> p = nullptr).
    constexpr SharedPtr(std::nullptr_t) noexcept {}

    // Конструктор от обычного указателя: берет объект во владение.
    // U может быть наследником T. explicit - чтобы владение не захватывалось случайно.
    // Если не удалось выделить управляющий блок, объект удаляется (иначе была бы утечка).
    template <class U>
        requires std::is_convertible_v<U*, T*>
    explicit SharedPtr(U* ptr) {
        if (!ptr) return;
        try {
            control_ = new detail::PointerControlBlock<U>(ptr);
        } catch (...) {
            delete ptr;
            throw;
        }
        ptr_ = ptr;
    }

    // Копирующий конструктор: становимся еще одним владельцем того же объекта.
    SharedPtr(const SharedPtr& other) noexcept : ptr_(other.ptr_), control_(other.control_) {
        if (control_) control_->addRef();
    }

    // Копирующий конструктор из SharedPtr другого типа
    // (например SharedPtr<Derived> -> SharedPtr<Base> или SharedPtr<T> -> SharedPtr<const T>).
    template <class U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr(const SharedPtr<U>& other) noexcept : ptr_(other.ptr_), control_(other.control_) {
        if (control_) control_->addRef();
    }

    // Перемещающий конструктор: забираем владение у other, other становится пустым.
    // Счетчик не меняется - число владельцев осталось тем же.
    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)), control_(std::exchange(other.control_, nullptr)) {}

    // Перемещающий конструктор из SharedPtr другого (совместимого) типа.
    template <class U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr(SharedPtr<U>&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)), control_(std::exchange(other.control_, nullptr)) {}

    // Деструктор: отказываемся от владения; если были последним владельцем - объект удаляется.
    ~SharedPtr() {
        if (control_) control_->release();
    }

    // Копирующее присваивание. Идиома copy-and-swap: делаем временную копию other
    // и меняемся с ней содержимым; старое значение уходит во временный объект и
    // освобождается в его деструкторе. Так самоприсваивание (p = p) безопасно.
    SharedPtr& operator=(const SharedPtr& other) noexcept {
        SharedPtr(other).swap(*this);
        return *this;
    }

    // Копирующее присваивание из SharedPtr совместимого типа.
    template <class U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr& operator=(const SharedPtr<U>& other) noexcept {
        SharedPtr(other).swap(*this);
        return *this;
    }

    // Перемещающее присваивание: забираем владение у other, старое значение освобождается.
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        SharedPtr(std::move(other)).swap(*this);
        return *this;
    }

    // Перемещающее присваивание из SharedPtr совместимого типа.
    template <class U>
        requires std::is_convertible_v<U*, T*>
    SharedPtr& operator=(SharedPtr<U>&& other) noexcept {
        SharedPtr(std::move(other)).swap(*this);
        return *this;
    }

    // Присваивание nullptr: то же, что reset().
    SharedPtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    // Отказаться от владения, указатель становится пустым.
    void reset() noexcept { SharedPtr().swap(*this); }

    // Отказаться от текущего объекта и взять во владение новый.
    template <class U>
        requires std::is_convertible_v<U*, T*>
    void reset(U* ptr) {
        SharedPtr(ptr).swap(*this);
    }

    // Обменяться содержимым с other (без изменения счетчиков).
    void swap(SharedPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(control_, other.control_);
    }

    // Обычный (сырой) указатель на объект; владение не передается.
    T* get() const noexcept { return ptr_; }

    // Разыменование, вид 1: *p - ссылка на объект.
    T& operator*() const noexcept { return *ptr_; }

    // Разыменование, вид 2: p->member - доступ к членам объекта.
    T* operator->() const noexcept { return ptr_; }

    // Сколько SharedPtr сейчас владеют объектом (0 для пустого указателя).
    long use_count() const noexcept {
        return control_ ? control_->strong.load(std::memory_order_relaxed) : 0;
    }

    // Являемся ли мы единственным владельцем.
    bool unique() const noexcept { return use_count() == 1; }

    // Проверка на непустоту: if (p) { ... }.
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    // SharedPtr<U> других типов имеют доступ к полям (нужно для преобразований Derived -> Base).
    template <class U>
    friend class SharedPtr;

    // makeShared использует закрытый конструктор ниже.
    template <class U, class... Args>
    friend SharedPtr<U> makeShared(Args&&... args);

    // Закрытый конструктор от готовых объекта и блока (счетчик в блоке уже равен 1).
    SharedPtr(T* ptr, detail::ControlBlockBase* control) noexcept : ptr_(ptr), control_(control) {}

    T* ptr_ = nullptr;                             // указатель на объект
    detail::ControlBlockBase* control_ = nullptr;  // общий управляющий блок
};

// Создает объект T с аргументами args и возвращает владеющий им SharedPtr.
// Объект и счетчик лежат в одном блоке памяти (аналог std::make_shared).
template <class T, class... Args>
SharedPtr<T> makeShared(Args&&... args) {
    auto* block = new detail::InplaceControlBlock<T>(std::forward<Args>(args)...);
    return SharedPtr<T>(block->get(), block);
}

// Свободная функция swap (чтобы работали swap(a, b) и стандартные алгоритмы).
template <class T>
void swap(SharedPtr<T>& a, SharedPtr<T>& b) noexcept {
    a.swap(b);
}

// Сравнение указателей: равны, если указывают на один и тот же объект.
// Оператор != компилятор выводит из == автоматически (C++20).
template <class T, class U>
bool operator==(const SharedPtr<T>& a, const SharedPtr<U>& b) noexcept {
    return a.get() == b.get();
}

// Упорядочивание по адресам объектов. Из <=> компилятор выводит <, <=, >, >=.
template <class T, class U>
std::strong_ordering operator<=>(const SharedPtr<T>& a, const SharedPtr<U>& b) noexcept {
    using P = std::common_type_t<T*, U*>;
    return std::compare_three_way{}(static_cast<P>(a.get()), static_cast<P>(b.get()));
}

// Сравнение с nullptr: p == nullptr, nullptr != p и т.п.
template <class T>
bool operator==(const SharedPtr<T>& a, std::nullptr_t) noexcept {
    return !a;
}

// Упорядочивание относительно nullptr: p < nullptr и т.п.
template <class T>
std::strong_ordering operator<=>(const SharedPtr<T>& a, std::nullptr_t) noexcept {
    return std::compare_three_way{}(a.get(), static_cast<T*>(nullptr));
}

}  // namespace smart

// Хэш по адресу объекта: позволяет хранить SharedPtr в unordered_set / unordered_map.
template <class T>
struct std::hash<smart::SharedPtr<T>> {
    std::size_t operator()(const smart::SharedPtr<T>& p) const noexcept {
        return std::hash<T*>{}(p.get());
    }
};
