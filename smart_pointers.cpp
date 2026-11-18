#include <iostream>
#include <string>
#include <utility>
#include <cstddef>
#include <windows.h>

using namespace std;

// Допоміжний клас для наочного відстеження створення та знищення об'єктів у пам'яті
class Resource {
private:
    string name;

public:
    Resource(const string& name) : name(name) {
        cout << "   [+] Створено Resource: \"" << this->name << "\" (адреса: " << this << ")\n";
    }

    ~Resource() {
        cout << "   [-] Знищено Resource: \"" << this->name << "\" (адреса: " << this << ")\n";
    }

    void sayHello() const {
        cout << "   >>> Привіт від ресурсу \"" << name << "\"!\n";
    }

    void setName(const string& newName) {
        name = newName;
    }

    const string& getName() const {
        return name;
    }
};

// =========================================================
// 1. Аналог std::unique_ptr (ексклюзивне володіння)
// =========================================================
template <typename T>
class UniquePtr {
private:
    T* ptr;

public:
    // Конструктори
    constexpr UniquePtr() noexcept : ptr(nullptr) {}
    constexpr UniquePtr(nullptr_t) noexcept : ptr(nullptr) {}
    explicit UniquePtr(T* rawPtr) noexcept : ptr(rawPtr) {}

    // Заборона копіювання
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // Конструктор переміщення
    UniquePtr(UniquePtr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    // Оператор присвоєння переміщенням
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    // Деструктор
    ~UniquePtr() {
        delete ptr;
    }

    // Оператори розіменування та доступу до членів
    T& operator*() const noexcept {
        return *ptr;
    }

    T* operator->() const noexcept {
        return ptr;
    }

    // Перевірка на наявність вказівника в умовах (if (uPtr))
    explicit operator bool() const noexcept {
        return ptr != nullptr;
    }

    // Отримання сирого вказівника без втрати володіння
    T* get() const noexcept {
        return ptr;
    }

    // Відмова від володіння без видалення об'єкта
    T* release() noexcept {
        T* temp = ptr;
        ptr = nullptr;
        return temp;
    }

    // Заміна керованого об'єкта (старий об'єкт видаляється)
    void reset(T* newPtr = nullptr) noexcept {
        if (ptr != newPtr) {
            delete ptr;
            ptr = newPtr;
        }
    }

    // Обмін вмістом із іншим UniquePtr
    void swap(UniquePtr& other) noexcept {
        std::swap(ptr, other.ptr);
    }
};

// Допоміжна функція-фабрика (аналог std::make_unique)
template <typename T, typename... Args>
UniquePtr<T> makeUnique(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

// =========================================================
// 2. Аналог std::shared_ptr (спільне володіння з підрахунком посилань)
// =========================================================
template <typename T>
class SharedPtr {
private:
    T* ptr;
    size_t* refCount;

    // Зменшення лічильника та звільнення пам'яті, якщо власників більше немає
    void releaseOwnership() noexcept {
        if (refCount != nullptr) {
            (*refCount)--;
            if (*refCount == 0) {
                delete ptr;
                delete refCount;
            }
            ptr = nullptr;
            refCount = nullptr;
        }
    }

public:
    // Конструктори
    constexpr SharedPtr() noexcept : ptr(nullptr), refCount(nullptr) {}
    constexpr SharedPtr(nullptr_t) noexcept : ptr(nullptr), refCount(nullptr) {}

    explicit SharedPtr(T* rawPtr) : ptr(rawPtr), refCount(nullptr) {
        if (rawPtr != nullptr) {
            refCount = new size_t(1);
        }
    }

    // Конструктор копіювання (збільшує лічильник посилань)
    SharedPtr(const SharedPtr& other) noexcept : ptr(other.ptr), refCount(other.refCount) {
        if (refCount != nullptr) {
            (*refCount)++;
        }
    }

    // Конструктор переміщення (передає володіння без зміни лічильника)
    SharedPtr(SharedPtr&& other) noexcept : ptr(other.ptr), refCount(other.refCount) {
        other.ptr = nullptr;
        other.refCount = nullptr;
    }

    // Оператор присвоєння копіюванням
    SharedPtr& operator=(const SharedPtr& other) noexcept {
        if (this != &other) {
            releaseOwnership();
            ptr = other.ptr;
            refCount = other.refCount;
            if (refCount != nullptr) {
                (*refCount)++;
            }
        }
        return *this;
    }

    // Оператор присвоєння переміщенням
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            releaseOwnership();
            ptr = other.ptr;
            refCount = other.refCount;
            other.ptr = nullptr;
            other.refCount = nullptr;
        }
        return *this;
    }

    // Деструктор
    ~SharedPtr() {
        releaseOwnership();
    }

    // Оператори доступу
    T& operator*() const noexcept {
        return *ptr;
    }

    T* operator->() const noexcept {
        return ptr;
    }

    explicit operator bool() const noexcept {
        return ptr != nullptr;
    }

    T* get() const noexcept {
        return ptr;
    }

    // Кількість активних SharedPtr, що володіють цим об'єктом
    size_t use_count() const noexcept {
        return refCount ? *refCount : 0;
    }

    // Перевірка, чи є поточний покажчик єдиним власником
    bool unique() const noexcept {
        return use_count() == 1;
    }

    // Скидання або заміна керованого об'єкта
    void reset(T* newPtr = nullptr) {
        releaseOwnership();
        if (newPtr != nullptr) {
            ptr = newPtr;
            refCount = new size_t(1);
        }
    }

    void swap(SharedPtr& other) noexcept {
        std::swap(ptr, other.ptr);
        std::swap(refCount, other.refCount);
    }
};

// Допоміжна функція-фабрика (аналог std::make_shared)
template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}

// =========================================================
// Тестування UniquePtr та SharedPtr
// =========================================================
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    cout << "================ 1. ТЕСТУВАННЯ UniquePtr ================\n";
    {
        UniquePtr<Resource> uPtr1 = makeUnique<Resource>("Підключення до БД");
        if (uPtr1) {
            uPtr1->sayHello();
            (*uPtr1).setName("Підключення до БД (оновлено)");
            cout << "   Нова назва через operator*: " << uPtr1->getName() << '\n';
        }

        // Копіювання заборонено на етапі компіляції:
        // UniquePtr<Resource> uCopy = uPtr1; // Помилка компіляції!

        cout << "\n   Переміщення володіння з uPtr1 у uPtr2 (std::move):\n";
        UniquePtr<Resource> uPtr2 = std::move(uPtr1);
        cout << "   uPtr1 порожній? " << (!uPtr1 ? "так (nullptr)" : "ні") << '\n';
        cout << "   uPtr2 володіє об'єктом: " << uPtr2->getName() << '\n';

        cout << "\n   Заміна об'єкта через reset():\n";
        uPtr2.reset(new Resource("Новий файловий потік"));

        cout << "\n   Вилучення сирого вказівника через release():\n";
        Resource* raw = uPtr2.release();
        cout << "   uPtr2 порожній після release()? " << (!uPtr2 ? "так" : "ні") << '\n';
        delete raw; // Оскільки release() зняв автоматичний контроль, видаляємо вручну

        cout << "   Вихід із блоку області видимості UniquePtr...\n";
    }

    cout << "\n================ 2. ТЕСТУВАННЯ SharedPtr ================\n";
    {
        SharedPtr<Resource> sPtr1 = makeShared<Resource>("Спільний конфігураційний буфер");
        cout << "   sPtr1 створено | use_count = " << sPtr1.use_count()
             << " | unique = " << (sPtr1.unique() ? "так" : "ні") << '\n';

        {
            cout << "\n   Вхід у внутрішній блок та створення копій sPtr2 і sPtr3:\n";
            SharedPtr<Resource> sPtr2 = sPtr1;
            SharedPtr<Resource> sPtr3;
            sPtr3 = sPtr2;

            cout << "   use_count після копіювання = " << sPtr1.use_count()
                 << " | unique = " << (sPtr1.unique() ? "так" : "ні") << '\n';

            sPtr3->setName("Спільний буфер (змінено через sPtr3)");
            cout << "   Перевірка через sPtr1: " << sPtr1->getName() << '\n';
            cout << "   Вихід із внутрішнього блоку (sPtr2 та sPtr3 знищуються, але об'єкт ще живий):\n";
        }

        cout << "   Після виходу з блоку | use_count = " << sPtr1.use_count()
             << " | unique = " << (sPtr1.unique() ? "так" : "ні") << '\n';

        cout << "\n   Переміщення sPtr1 у sPtr4 (лічильник не збільшується):\n";
        SharedPtr<Resource> sPtr4 = std::move(sPtr1);
        cout << "   sPtr1 use_count = " << sPtr1.use_count()
             << " | sPtr4 use_count = " << sPtr4.use_count() << '\n';

        cout << "   Вихід із зовнішнього блоку (знищується останній власник sPtr4):\n";
    }

    cout << "\nУсі ресурси успішно звільнені без витоків пам'яті.\n";
    return 0;
}