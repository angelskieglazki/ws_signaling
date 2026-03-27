# ws_signaling

## Сборка

### Требования
- CMake 3.16+
- Qt6 (для клиента)
- Компилятор с поддержкой C++17

### Настройка Qt пути

Клиент использует Qt6, путь к которому нужно указать. Для этого:

1. Скопируй шаблон локальных настроек:
   ```bash
   cp client/local.cmake.example client/local.cmake
   ```

2. Отредактируй `client/local.cmake`, указав путь к своей установке Qt:
   ```cmake
   set(CMAKE_PREFIX_PATH "/home/max/Qt/6.3.1/gcc_64")
   ```

Файл `local.cmake` игнорируется git, так что каждый разработчик может иметь свои локальные настройки.

### Сборка проекта

```bash
mkdir build && cd build
cmake ..
make
```

### Docker
```bash
docker build -t signaling-server .
docker run -d -p 9002:9002 signaling-server
```
