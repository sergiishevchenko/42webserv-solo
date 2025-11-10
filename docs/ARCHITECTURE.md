# Архитектура webserv: Взаимодействие компонентов

## Обзор компонентов

Проект состоит из четырех основных компонентов:
1. **ConfigParser** - парсинг конфигурационного файла
2. **Socket** - управление сетевыми сокетами
3. **Connection** - представление клиентских подключений
4. **Server** - оркестрация всех компонентов

---

## 1. ConfigParser (Парсер конфигурации)

### Назначение
Парсит конфигурационный файл в формате, похожем на nginx, и создает структуры данных `ServerConfig` и `Location`.

### Ключевые методы:
- `loadFromFile()` - загружает и парсит файл конфигурации
- `validate()` - валидирует распарсенную конфигурацию
- `getServers()` - возвращает вектор конфигураций серверов

### Структуры данных:
```cpp
struct ServerConfig {
    std::vector<std::pair<std::string, int>> listen;   // host:port пары
    std::string root;                                  // корневая директория
    std::string index;                                 // индексный файл
    size_t client_max_body_size;                       // максимальный размер тела запроса
    std::map<int, std::string> error_pages;            // страницы ошибок
    std::vector<Location> locations;                   // локации (маршруты)
};
```

### Процесс работы:
1. Читает файл построчно
2. Находит блоки `server { ... }`
3. Парсит директивы внутри блоков:
   - `listen` - адреса и порты для прослушивания
   - `root`, `index`, `client_max_body_size` - основные настройки
   - `location` - блоки с маршрутами
4. Валидирует конфигурацию (проверяет обязательные поля, порты и т.д.)

---

## 2. Socket (Сокет)

### Назначение
Класс `Socket` инкапсулирует работу с системными сокетами (socket API). Создает слушающие сокеты для сервера.

### Краткое описание

**Сокет** - это программный интерфейс для сетевого взаимодействия между программами. В проекте класс `Socket` используется для создания слушающих сокетов, которые ожидают входящие подключения от клиентов.

**Ключевые концепции:**
- **Слушающий сокет** - один на порт, живет постоянно, ждет новые подключения
- **Клиентский сокет** - создается для каждого клиента, используется для обмена данными
- **Файловый дескриптор (fd)** - число для работы с сокетом (3, 4, 5, ...)

### Ключевые методы:
- `bind(host, port)` - привязывает сокет к адресу и порту
- `listen(backlog)` - переводит сокет в режим прослушивания
- `accept()` - принимает новое подключение (возвращает файловый дескриптор клиента)
- `setNonBlocking()` - устанавливает неблокирующий режим
- `setCloseOnExec()` - устанавливает флаг FD_CLOEXEC

### Процесс создания слушающего сокета:
1. **socket()** - создает TCP сокет (AF_INET, SOCK_STREAM)
2. **setsockopt(SO_REUSEADDR)** - позволяет переиспользовать адрес
3. **setNonBlocking()** - делает сокет неблокирующим (важно для event loop)
4. **bind()** - привязывает к host:port
5. **listen()** - начинает прослушивать входящие подключения

### Важные детали:
- Сокет хранит свой файловый дескриптор (`fd_`)
- После `bind()` сокет готов принимать подключения через `accept()`
- Каждый `accept()` возвращает новый файловый дескриптор для клиента

### Подробная документация

Для подробного объяснения концепции сокетов, их жизненного цикла, работы с файловыми дескрипторами и неблокирующим режимом, см. **[SOCKETS.md](SOCKETS.md)** (на английском языке).

Там вы найдете:
- Детальное объяснение что такое сокет
- Жизненный цикл сокета на сервере (socket → bind → listen → accept)
- Различия между слушающим и клиентским сокетами
- Как работает передача данных через сокеты
- Неблокирующий режим и event-driven архитектура
- Примеры кода из проекта

---

## 3. Connection (Подключение)

### Назначение
Представляет активное клиентское подключение. Хранит информацию о клиенте и управляет его файловым дескриптором.

### Поля:
- `fd_` - файловый дескриптор сокета клиента
- `client_ip_` - IP-адрес клиента
- `last_activity_` - время последней активности (для таймаутов)

### Методы:
- `updateActivity()` - обновляет время последней активности
- `close()` - закрывает соединение
- `getFd()`, `getClientIp()`, `getLastActivity()` - геттеры

### Жизненный цикл:
1. Создается когда Server принимает новое подключение
2. Хранится в `Server::connections_` map (ключ - файловый дескриптор)
3. Удаляется при закрытии соединения или таймауте

---

## 4. Server (Сервер)

### Назначение
Оркестрирует всю работу сервера: инициализация, event loop, управление подключениями.

### Ключевые компоненты:
- `listening_sockets_` - вектор слушающих сокетов (по одному на каждый host:port)
- `connections_` - map активных подключений (fd -> Connection*)
- `poll_fds_` - вектор структур pollfd для event loop
- `running_` - флаг работы сервера

### Основные методы:

#### `init(const ConfigParser& config)`
1. Получает конфигурации серверов из ConfigParser
2. Для каждого `listen` адреса создает Socket
3. Вызывает `bind()` и `listen()` на каждом сокете
4. Добавляет сокеты в `listening_sockets_`
5. Регистрирует файловые дескрипторы в `poll_fds_` для мониторинга

#### `run()` - Event Loop
1. Основной цикл сервера
2. На каждой итерации:
   - Вызывает `setupPollFds()` - обновляет список файловых дескрипторов для мониторинга
   - Вызывает `poll()` - ждет событий (с таймаутом 1 секунда)
   - Обрабатывает события через `handlePollEvents()`
   - Очищает устаревшие подключения через `cleanupTimedOutConnections()`

#### `handlePollEvents()`
Обрабатывает события от `poll()`:
- **POLLIN на слушающем сокете** → вызывает `acceptNewConnection()`
- **POLLIN на клиентском сокете** → вызывает `handleClientRead()`
- **POLLOUT на клиентском сокете** → вызывает `handleClientWrite()`
- **POLLERR/POLLHUP** → закрывает соединение

#### `acceptNewConnection(Socket* socket)`
1. Вызывает `socket->accept()` - получает новый файловый дескриптор клиента
2. Устанавливает неблокирующий режим для клиентского сокета
3. Получает IP-адрес клиента через `getpeername()`
4. Создает объект `Connection`
5. Добавляет в `connections_` map
6. Клиентский сокет будет добавлен в `poll_fds_` при следующем `setupPollFds()`

#### `handleClientRead(int fd)`
1. Находит Connection по файловому дескриптору
2. Обновляет время активности
3. Читает данные через `recv()`
4. Обрабатывает запрос (сейчас просто отправляет "Hello, World!")
5. Закрывает соединение после отправки ответа

---

## Поток взаимодействия компонентов

### Инициализация (main.cpp):
```
main()
  ↓
ConfigParser::loadFromFile()  → Парсит конфигурационный файл
  ↓
ConfigParser::validate()      → Проверяет корректность конфигурации
  ↓
Server::init(parser)          → Инициализирует сервер
  ├─ Получает ServerConfig из parser
  ├─ Для каждого listen адреса:
  │   ├─ Создает Socket
  │   ├─ Socket::bind(host, port)
  │   ├─ Socket::listen()
  │   └─ Добавляет в listening_sockets_
  └─ Регистрирует сокеты в poll_fds_
  ↓
Server::run()                 → Запускает event loop
```

### Обработка подключений (Event Loop):
```
Server::run()
  ↓
setupPollFds()                → Собирает все fd для мониторинга
  ├─ listening sockets (POLLIN)
  └─ client connections (POLLIN | POLLOUT)
  ↓
poll(poll_fds_, timeout)      → Ждет событий
  ↓
handlePollEvents()            → Обрабатывает события
  ├─ Если POLLIN на listening socket:
  │   └─ acceptNewConnection()
  │       ├─ Socket::accept() → новый client_fd
  │       ├─ Создает Connection(client_fd, client_ip)
  │       └─ Добавляет в connections_ map
  │
  └─ Если POLLIN на client socket:
      └─ handleClientRead()
          ├─ Connection::updateActivity()
          ├─ recv() → читает данные
          ├─ send() → отправляет ответ
          └─ closeConnection() → удаляет из connections_
```

### Управление подключениями:
```
Connection создается
  ↓
Хранится в Server::connections_ map
  ↓
Мониторится через poll() в poll_fds_
  ↓
При активности: Connection::updateActivity()
  ↓
При таймауте или закрытии: Connection::close() → удаление из map
```

---

## Важные детали реализации

### Event-driven архитектура
- Используется `poll()` для мониторинга множества файловых дескрипторов
- Все сокеты в неблокирующем режиме (O_NONBLOCK)
- Это позволяет обрабатывать множество подключений в одном потоке

### Управление жизненным циклом
- `Socket` объекты создаются в `Server::init()` и удаляются в `Server::stop()`
- `Connection` объекты создаются при `accept()` и удаляются при закрытии
- Все файловые дескрипторы закрываются в деструкторах

### Таймауты
- Каждое подключение имеет `last_activity_`
- `cleanupTimedOutConnections()` проверяет все подключения
- Подключения без активности > 60 секунд закрываются

### Множественные адреса
- Один ServerConfig может иметь несколько `listen` директив
- Для каждого создается отдельный Socket
- Все слушающие сокеты мониторятся одновременно через poll()

---

## Пример работы (детальный пошаговый разбор)

### Конфигурация:
```
server {
    listen 127.0.0.1:8080;
    listen 0.0.0.0:8081;
    root www;
    index index.html;
    client_max_body_size 10485760;
}
```

---

## ФАЗА 1: ИНИЦИАЛИЗАЦИЯ

### Шаг 1.1: Запуск программы
```bash
./webserv config/example.conf
```

**Что происходит:**
- Вызывается `main(int argc, char** argv)`
- `argc = 2`, `argv[1] = "config/example.conf"`

### Шаг 1.2: Создание ConfigParser
```cpp
ConfigParser parser;
```

**Состояние объекта:**
```
ConfigParser:
  servers_: [] (пустой вектор)
  lastError_: "" (пустая строка)
```

### Шаг 1.3: Загрузка конфигурационного файла
```cpp
parser.loadFromFile("config/example.conf")
```

**Что происходит внутри:**

1. **Открытие файла:**
   - `std::ifstream file("config/example.conf")`
   - Файл успешно открыт

2. **Чтение первой строки:**
   - Строка: `"server {"`
   - `trimmed.find("server") == 0` → найдено начало блока сервера

3. **Создание ServerConfig:**
   ```cpp
   ServerConfig server;
   // server.listen = []
   // server.root = ""
   // server.index = ""
   // server.client_max_body_size = 1048576 (по умолчанию)
   // server.error_pages = {}
   // server.locations = []
   ```

4. **Парсинг директивы `listen 127.0.0.1:8080;`:**
   - `parseListen("listen 127.0.0.1:8080;", server)`
   - Находит двоеточие на позиции 12
   - `interface = "127.0.0.1"`, `portStr = "8080"`
   - `port = 8080` (валидный порт)
   - `server.listen.push_back(std::make_pair("127.0.0.1", 8080))`
   
   **Состояние ServerConfig:**
   ```
   server.listen = [("127.0.0.1", 8080)]
   ```

5. **Парсинг директивы `listen 0.0.0.0:8081;`:**
   - `parseListen("listen 0.0.0.0:8081;", server)`
   - `server.listen.push_back(std::make_pair("0.0.0.0", 8081))`
   
   **Состояние ServerConfig:**
   ```
   server.listen = [("127.0.0.1", 8080), ("0.0.0.0", 8081)]
   ```

6. **Парсинг директивы `root www;`:**
   - `parseDirective("root www;", server)`
   - `server.root = "www"`

7. **Парсинг директивы `index index.html;`:**
   - `parseDirective("index index.html;", server)`
   - `server.index = "index.html"`

8. **Парсинг директивы `client_max_body_size 10485760;`:**
   - `parseDirective("client_max_body_size 10485760;", server)`
   - `server.client_max_body_size = 10485760`

9. **Завершение парсинга блока:**
   - Встречена закрывающая скобка `}`
   - `servers_.push_back(server)`

**Финальное состояние ConfigParser:**
```
ConfigParser:
  servers_: [
    ServerConfig {
      listen: [("127.0.0.1", 8080), ("0.0.0.0", 8081)]
      root: "www"
      index: "index.html"
      client_max_body_size: 10485760
      error_pages: {}
      locations: []
    }
  ]
  lastError_: ""
```

### Шаг 1.4: Валидация конфигурации
```cpp
parser.validate()
```

**Что проверяется:**
1. ✅ `servers_.empty() == false` - есть хотя бы один сервер
2. ✅ `server.listen.empty() == false` - есть listen директивы
3. ✅ Порты валидны (8080 и 8081 в диапазоне 1-65535)
4. ✅ `server.root.empty() == false` - root указан
5. ✅ `server.client_max_body_size > 0` - размер тела запроса валиден

**Результат:** `validate() возвращает true`

### Шаг 1.5: Создание Server объекта
```cpp
Server server;
```

**Состояние объекта:**
```
Server:
  listening_sockets_: [] (пустой вектор)
  connections_: {} (пустая map)
  poll_fds_: [] (пустой вектор)
  running_: false
  connection_timeout_: 60
```

### Шаг 1.6: Инициализация сервера
```cpp
server.init(parser)
```

**Что происходит внутри:**

#### 1.6.1: Получение конфигураций
```cpp
const std::vector<ServerConfig>& servers = config.getServers();
// servers.size() == 1
// servers[0].listen.size() == 2
```

#### 1.6.2: Создание первого Socket (127.0.0.1:8080)

**Шаг 1.6.2.1: Создание Socket объекта**
```cpp
Socket* socket = new Socket();
```

**Состояние Socket:**
```
Socket:
  fd_: -1 (неинициализирован)
  host_: ""
  port_: 0
```

**Шаг 1.6.2.2: Вызов bind("127.0.0.1", 8080)**

**Внутри Socket::bind():**

1. **Создание сокета:**
   ```cpp
   fd_ = socket(AF_INET, SOCK_STREAM, 0);
   // Системный вызов: socket(AF_INET, SOCK_STREAM, 0)
   // Возвращает: fd = 3 (например)
   ```

2. **Установка SO_REUSEADDR:**
   ```cpp
   setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
   // Системный вызов: setsockopt(3, SOL_SOCKET, SO_REUSEADDR, ...)
   // Позволяет переиспользовать адрес после перезапуска
   ```

3. **Установка неблокирующего режима:**
   ```cpp
   setNonBlocking();
   // fcntl(3, F_GETFL) → получает текущие флаги
   // fcntl(3, F_SETFL, flags | O_NONBLOCK) → устанавливает O_NONBLOCK
   // Системный вызов: fcntl(3, F_SETFL, O_RDWR | O_NONBLOCK)
   ```

4. **Установка FD_CLOEXEC:**
   ```cpp
   setCloseOnExec();
   // fcntl(3, F_GETFD) → получает флаги файлового дескриптора
   // fcntl(3, F_SETFD, flags | FD_CLOEXEC) → устанавливает FD_CLOEXEC
   ```

5. **Подготовка адреса:**
   ```cpp
   struct sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_port = htons(8080);  // Конвертация в network byte order
   // host == "127.0.0.1" (не "0.0.0.0")
   inet_aton("127.0.0.1", &addr.sin_addr);
   // addr.sin_addr.s_addr = 0x0100007f (127.0.0.1 в network byte order)
   ```

6. **Привязка сокета:**
   ```cpp
   bind(fd_, (struct sockaddr*)&addr, sizeof(addr));
   // Системный вызов: bind(3, (struct sockaddr*)&addr, sizeof(addr))
   // Привязывает сокет к 127.0.0.1:8080
   ```

7. **Сохранение параметров:**
   ```cpp
   host_ = "127.0.0.1";
   port_ = 8080;
   ```

**Состояние Socket после bind:**
```
Socket:
  fd_: 3
  host_: "127.0.0.1"
  port_: 8080
```

**Шаг 1.6.2.3: Вызов listen()**
```cpp
socket->listen();
```

**Внутри Socket::listen():**
```cpp
listen(fd_, 128);
// Системный вызов: listen(3, 128)
// Переводит сокет в режим прослушивания
// backlog = 128 (максимум ожидающих подключений в очереди)
```

**Состояние системы:**
- Сокет fd=3 теперь слушает на 127.0.0.1:8080
- ОС готова принимать входящие подключения на этот адрес

**Шаг 1.6.2.4: Добавление в listening_sockets_**
```cpp
listening_sockets_.push_back(socket);
```

**Состояние Server:**
```
Server:
  listening_sockets_: [Socket* (fd=3, host="127.0.0.1", port=8080)]
  connections_: {}
  poll_fds_: []
```

**Шаг 1.6.2.5: Регистрация в poll_fds_**
```cpp
addPollFd(socket->getFd(), POLLIN);
```

**Внутри addPollFd():**
```cpp
struct pollfd pfd;
pfd.fd = 3;
pfd.events = POLLIN;  // Мониторим события на чтение
pfd.revents = 0;
poll_fds_.push_back(pfd);
```

**Состояние Server:**
```
Server:
  listening_sockets_: [Socket* (fd=3)]
  connections_: {}
  poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0}
  ]
```

**Лог:**
```
[INFO] Listening on 127.0.0.1:8080
```

#### 1.6.3: Создание второго Socket (0.0.0.0:8081)

**Аналогичные шаги для второго сокета:**

1. `Socket* socket2 = new Socket();`
2. `socket2->bind("0.0.0.0", 8081)`
   - `socket(AF_INET, SOCK_STREAM, 0)` → `fd = 4`
   - `setsockopt(4, SOL_SOCKET, SO_REUSEADDR, ...)`
   - `fcntl(4, F_SETFL, O_NONBLOCK)`
   - `addr.sin_addr.s_addr = INADDR_ANY` (0.0.0.0)
   - `bind(4, ...)` → привязка к 0.0.0.0:8081
3. `socket2->listen()` → `listen(4, 128)`
4. `listening_sockets_.push_back(socket2)`
5. `addPollFd(4, POLLIN)`

**Финальное состояние Server после init:**
```
Server:
  listening_sockets_: [
    Socket* (fd=3, host="127.0.0.1", port=8080),
    Socket* (fd=4, host="0.0.0.0", port=8081)
  ]
  connections_: {} (пустая map)
  poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0}
  ]
  running_: false
  connection_timeout_: 60
```

**Лог:**
```
[INFO] Listening on 127.0.0.1:8080
[INFO] Listening on 0.0.0.0:8081
```

---

## ФАЗА 2: ЗАПУСК EVENT LOOP

### Шаг 2.1: Запуск сервера
```cpp
server.run()
```

**Установка флага:**
```cpp
running_ = true;
```

**Лог:**
```
[INFO] Server started. Waiting for connections...
```

### Шаг 2.2: Первая итерация event loop

#### 2.2.1: Настройка poll файловых дескрипторов
```cpp
setupPollFds()
```

**Что происходит:**
1. `poll_fds_.clear()` - очистка (на самом деле не нужна в первой итерации)
2. Добавление слушающих сокетов:
   ```cpp
   for (Socket* sock : listening_sockets_) {
       addPollFd(sock->getFd(), POLLIN);
   }
   ```
3. Добавление клиентских подключений (пока пусто):
   ```cpp
   for (auto& pair : connections_) {
       addPollFd(pair.first, POLLIN | POLLOUT);
   }
   // connections_ пуста, поэтому ничего не добавляется
   ```

**Состояние poll_fds_:**
```
poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0}
]
```

#### 2.2.2: Вызов poll()
```cpp
int poll_result = poll(&poll_fds_[0], poll_fds_.size(), 1000);
// poll_result = 0 (таймаут, событий нет)
// timeout = 1000ms (1 секунда)
```

**Что происходит:**
- Системный вызов `poll()` блокируется на 1 секунду
- За это время нет входящих подключений
- `poll()` возвращает 0 (таймаут)

#### 2.2.3: Обработка результата poll()
```cpp
if (poll_result == 0) {
    cleanupTimedOutConnections();  // Проверка таймаутов
    continue;  // Переход к следующей итерации
}
```

**cleanupTimedOutConnections():**
- `connections_` пуста, ничего не делается

**Цикл продолжается...**

---

## ФАЗА 3: ПОДКЛЮЧЕНИЕ КЛИЕНТА

### Шаг 3.1: Клиент инициирует подключение
```bash
# В другом терминале или браузере:
curl http://127.0.0.1:8080/
# или
telnet 127.0.0.1 8080
```

**Что происходит на уровне ОС:**
1. Клиент создает сокет и выполняет `connect(127.0.0.1:8080)`
2. ОС добавляет запрос на подключение в очередь сокета fd=3
3. Сокет становится "готовым к чтению" (можно принять подключение)

### Шаг 3.2: Итерация event loop с событием

#### 3.2.1: setupPollFds()
**Состояние poll_fds_:**
```
poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0}
]
```

#### 3.2.2: Вызов poll()
```cpp
int poll_result = poll(&poll_fds_[0], 2, 1000);
// Системный вызов: poll([{fd:3, events:POLLIN}, {fd:4, events:POLLIN}], 2, 1000)
```

**Что происходит:**
- `poll()` обнаруживает, что fd=3 готов к чтению (есть ожидающее подключение)
- `poll()` возвращает 1 (одно событие)
- ОС устанавливает `poll_fds_[0].revents = POLLIN`

**Состояние poll_fds_ после poll():**
```
poll_fds_: [
    {fd: 3, events: POLLIN, revents: POLLIN},  ← событие!
    {fd: 4, events: POLLIN, revents: 0}
]
```

#### 3.2.3: Обработка событий
```cpp
handlePollEvents()
```

**Что происходит внутри:**

1. **Итерация по poll_fds_:**
   ```cpp
   for (size_t i = 0; i < poll_fds_.size(); ++i) {
       struct pollfd& pfd = poll_fds_[i];
       // i == 0, pfd.fd == 3, pfd.revents == POLLIN
   ```

2. **Проверка ошибок:**
   ```cpp
   if (pfd.revents & POLLERR) { ... }  // false
   if (pfd.revents & POLLHUP) { ... }  // false
   ```

3. **Определение типа сокета:**
   ```cpp
   bool is_listening = false;
   for (Socket* sock : listening_sockets_) {
       if (sock->getFd() == pfd.fd) {  // sock->getFd() == 3
           is_listening = true;  // Это слушающий сокет!
           break;
       }
   }
   ```

4. **Обработка слушающего сокета:**
   ```cpp
   if (is_listening) {
       if (pfd.revents & POLLIN) {  // true!
           acceptNewConnection(listening_sockets_[0]);
       }
   }
   ```

#### 3.2.4: Принятие нового подключения
```cpp
acceptNewConnection(listening_sockets_[0])
```

**Что происходит внутри:**

1. **Вызов accept():**
   ```cpp
   int client_fd = socket->accept();
   // Внутри Socket::accept():
   // struct sockaddr_in client_addr;
   // socklen_t client_len = sizeof(client_addr);
   // int client_fd = accept(fd_, (struct sockaddr*)&client_addr, &client_len);
   // Системный вызов: accept(3, &client_addr, &client_len)
   // Возвращает: client_fd = 5 (новый файловый дескриптор)
   ```

2. **Установка неблокирующего режима:**
   ```cpp
   fcntl(client_fd, F_SETFL, O_NONBLOCK);
   // Системный вызов: fcntl(5, F_SETFL, O_RDWR | O_NONBLOCK)
   // Клиентский сокет теперь неблокирующий
   ```

3. **Получение IP-адреса клиента:**
   ```cpp
   struct sockaddr_in client_addr;
   socklen_t client_len = sizeof(client_addr);
   getpeername(client_fd, (struct sockaddr*)&client_addr, &client_len);
   // Системный вызов: getpeername(5, &client_addr, &client_len)
   // client_addr.sin_addr содержит IP клиента
   client_ip = inet_ntoa(client_addr.sin_addr);
   // client_ip = "127.0.0.1" (или IP клиента)
   ```

4. **Создание Connection объекта:**
   ```cpp
   Connection* conn = new Connection(client_fd, client_ip);
   ```

   **Внутри Connection конструктора:**
   ```cpp
   Connection::Connection(int fd, const std::string& client_ip)
       : fd_(fd), client_ip_(client_ip), last_activity_(time(NULL)) {}
   // fd_ = 5
   // client_ip_ = "127.0.0.1"
   // last_activity_ = текущее время (например, 1699123456)
   ```

   **Состояние Connection:**
   ```
   Connection:
     fd_: 5
     client_ip_: "127.0.0.1"
     last_activity_: 1699123456
   ```

5. **Добавление в connections_ map:**
   ```cpp
   connections_[client_fd] = conn;
   // connections_[5] = Connection* (fd=5, ip="127.0.0.1")
   ```

**Состояние Server после acceptNewConnection:**
```
Server:
  listening_sockets_: [
    Socket* (fd=3),
    Socket* (fd=4)
  ]
  connections_: {
    5: Connection* (fd=5, client_ip="127.0.0.1", last_activity=1699123456)
  }
  poll_fds_: [
    {fd: 3, events: POLLIN, revents: POLLIN},
    {fd: 4, events: POLLIN, revents: 0}
  ]
```

**Лог:**
```
[INFO] New connection from 127.0.0.1 (fd: 5)
```

6. **Очистка таймаутов:**
   ```cpp
   cleanupTimedOutConnections();
   // connections_[5]->last_activity_ = 1699123456 (текущее время)
   // Таймаут не истек, соединение не закрывается
   ```

---

## ФАЗА 4: ОБРАБОТКА ДАННЫХ ОТ КЛИЕНТА

### Шаг 4.1: Клиент отправляет HTTP запрос
```
GET / HTTP/1.1
Host: 127.0.0.1:8080
User-Agent: curl/7.68.0
Accept: */*

```

**Что происходит на уровне ОС:**
- Данные поступают в буфер сокета fd=5
- Сокет становится "готовым к чтению"

### Шаг 4.2: Итерация event loop с данными

#### 4.2.1: setupPollFds()
```cpp
setupPollFds()
```

**Что происходит:**
1. Очистка `poll_fds_`
2. Добавление слушающих сокетов:
   ```cpp
   addPollFd(3, POLLIN);  // Сокет 127.0.0.1:8080
   addPollFd(4, POLLIN);  // Сокет 0.0.0.0:8081
   ```
3. Добавление клиентских подключений:
   ```cpp
   for (auto& pair : connections_) {
       addPollFd(pair.first, POLLIN | POLLOUT);
       // addPollFd(5, POLLIN | POLLOUT);
   }
   ```

**Состояние poll_fds_:**
```
poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0},
    {fd: 5, events: POLLIN | POLLOUT, revents: 0}  ← клиентский сокет
]
```

#### 4.2.2: Вызов poll()
```cpp
int poll_result = poll(&poll_fds_[0], 3, 1000);
```

**Что происходит:**
- `poll()` обнаруживает, что fd=5 готов к чтению (есть данные)
- `poll()` возвращает 1
- ОС устанавливает `poll_fds_[2].revents = POLLIN`

**Состояние poll_fds_ после poll():**
```
poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0},
    {fd: 5, events: POLLIN | POLLOUT, revents: POLLIN}  ← событие!
]
```

#### 4.2.3: Обработка событий
```cpp
handlePollEvents()
```

1. **Итерация по poll_fds_:**
   - i=0: fd=3, revents=0 → пропуск
   - i=1: fd=4, revents=0 → пропуск
   - i=2: fd=5, revents=POLLIN → обработка!

2. **Определение типа сокета:**
   ```cpp
   bool is_listening = false;
   for (Socket* sock : listening_sockets_) {
       if (sock->getFd() == 5) {  // false (5 не является слушающим сокетом)
           is_listening = true;
       }
   }
   // is_listening = false
   ```

3. **Обработка клиентского сокета:**
   ```cpp
   if (!is_listening) {
       if (pfd.revents & POLLIN) {  // true!
           handleClientRead(5);
       }
   }
   ```

#### 4.2.4: Чтение данных от клиента
```cpp
handleClientRead(5)
```

**Что происходит внутри:**

1. **Поиск Connection:**
   ```cpp
   std::map<int, Connection*>::iterator it = connections_.find(5);
   // it != connections_.end() → Connection найден
   Connection* conn = it->second;  // conn указывает на Connection(fd=5)
   ```

2. **Обновление времени активности:**
   ```cpp
   conn->updateActivity();
   // last_activity_ = time(NULL);  // Обновляется текущее время
   // last_activity_ = 1699123457 (новое время)
   ```

3. **Чтение данных:**
   ```cpp
   char buffer[4096];
   ssize_t bytes_read = recv(5, buffer, sizeof(buffer) - 1, 0);
   // Системный вызов: recv(5, buffer, 4095, 0)
   // bytes_read = 67 (размер HTTP запроса)
   ```

   **Содержимое buffer:**
   ```
   "GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nUser-Agent: curl/7.68.0\r\nAccept: */*\r\n\r\n"
   ```

4. **Обработка прочитанных данных:**
   ```cpp
   buffer[bytes_read] = '\0';  // Добавление нулевого терминатора
   LOG_DEBUG() << "Received " << bytes_read << " bytes from fd " << 5;
   ```

   **Лог:**
   ```
   [DEBUG] Received 67 bytes from fd 5
   ```

5. **Формирование ответа:**
   ```cpp
   std::string response = "HTTP/1.1 200 OK\r\n";
   response += "Content-Type: text/plain\r\n";
   response += "Content-Length: 13\r\n";
   response += "Connection: close\r\n";
   response += "\r\n";
   response += "Hello, World!";
   ```

   **Содержимое response:**
   ```
   "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nConnection: close\r\n\r\nHello, World!"
   ```

6. **Отправка ответа:**
   ```cpp
   ssize_t bytes_sent = send(5, response.c_str(), response.length(), 0);
   // Системный вызов: send(5, response, 89, 0)
   // bytes_sent = 89 (все данные отправлены)
   ```

   **Лог:**
   ```
   [DEBUG] Sent 89 bytes to fd 5
   ```

7. **Закрытие соединения:**
   ```cpp
   closeConnection(5);
   ```

#### 4.2.5: Закрытие соединения
```cpp
closeConnection(5)
```

**Что происходит внутри:**

1. **Поиск Connection:**
   ```cpp
   std::map<int, Connection*>::iterator it = connections_.find(5);
   // it != connections_.end() → Connection найден
   ```

2. **Удаление Connection:**
   ```cpp
   delete it->second;  // Вызывается деструктор Connection
   ```

   **Внутри Connection::~Connection():**
   ```cpp
   ~Connection() { close(); }
   void close() {
       if (fd_ >= 0) {
           ::close(fd_);  // Системный вызов: close(5)
           fd_ = -1;
       }
   }
   ```

   **Системный вызов:**
   ```
   close(5)  // Закрытие файлового дескриптора клиента
   ```

3. **Удаление из map:**
   ```cpp
   connections_.erase(it);
   // connections_.erase(5)
   ```

**Состояние Server после closeConnection:**
```
Server:
  listening_sockets_: [
    Socket* (fd=3),
    Socket* (fd=4)
  ]
  connections_: {}  ← Connection удален
  poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0},
    {fd: 5, events: POLLIN | POLLOUT, revents: POLLIN}
  ]
  // Примечание: fd=5 еще в poll_fds_, но будет удален в следующем setupPollFds()
```

**Лог:**
```
[DEBUG] Connection closed (fd: 5)
```

#### 4.2.6: Очистка таймаутов
```cpp
cleanupTimedOutConnections();
// connections_ пуста, ничего не делается
```

### Шаг 4.3: Следующая итерация event loop

#### 4.3.1: setupPollFds()
```cpp
setupPollFds()
```

**Что происходит:**
- `poll_fds_.clear()` - очистка
- Добавление только слушающих сокетов (клиентских подключений нет)

**Состояние poll_fds_:**
```
poll_fds_: [
    {fd: 3, events: POLLIN, revents: 0},
    {fd: 4, events: POLLIN, revents: 0}
]
```

**Сервер снова ждет новых подключений...**

---

## ФАЗА 5: ЗАВЕРШЕНИЕ РАБОТЫ

### Шаг 5.1: Получение сигнала завершения
```bash
# Пользователь нажимает Ctrl+C
# Или отправляется сигнал: kill -SIGINT <pid>
```

**Обработчик сигнала:**
```cpp
signalHandler(SIGINT)
{
    if (g_server) {
        g_server->stop();
    }
}
```

### Шаг 5.2: Остановка сервера
```cpp
server.stop()
```

**Что происходит:**

1. **Установка флага:**
   ```cpp
   running_ = false;
   ```

2. **Закрытие всех клиентских подключений:**
   ```cpp
   for (auto& pair : connections_) {
       delete pair.second;  // Удаление всех Connection объектов
   }
   connections_.clear();
   ```

3. **Очистка poll_fds_:**
   ```cpp
   poll_fds_.clear();
   ```

4. **Закрытие всех слушающих сокетов:**
   ```cpp
   for (size_t i = 0; i < listening_sockets_.size(); ++i) {
       delete listening_sockets_[i];  // Удаление Socket объектов
   }
   listening_sockets_.clear();
   ```

   **Внутри Socket::~Socket():**
   ```cpp
   ~Socket() { close(); }
   void close() {
       if (fd_ >= 0) {
           ::close(fd_);  // close(3), close(4)
           fd_ = -1;
       }
   }
   ```

   **Системные вызовы:**
   ```
   close(3)  // Закрытие сокета 127.0.0.1:8080
   close(4)  // Закрытие сокета 0.0.0.0:8081
   ```

5. **Выход из event loop:**
   ```cpp
   // В Server::run():
   while (running_) {  // running_ == false, цикл завершается
       // ...
   }
   ```

**Лог:**
```
[INFO] Server stopped
```

### Шаг 5.3: Завершение программы
```cpp
return 0;  // main() завершается
```

---

## РЕЗЮМЕ ПОШАГОВОГО ПРОЦЕССА

1. **Инициализация:**
   - Парсинг конфигурации → ServerConfig структуры
   - Создание Socket объектов → bind() + listen()
   - Регистрация в poll_fds_

2. **Event Loop:**
   - setupPollFds() → сбор всех fd для мониторинга
   - poll() → ожидание событий
   - handlePollEvents() → обработка событий

3. **Новое подключение:**
   - poll() обнаруживает POLLIN на слушающем сокете
   - accept() → новый client_fd
   - Создание Connection объекта
   - Добавление в connections_ map

4. **Обработка данных:**
   - poll() обнаруживает POLLIN на клиентском сокете
   - recv() → чтение данных
   - send() → отправка ответа
   - closeConnection() → закрытие соединения

5. **Завершение:**
   - Сигнал → stop()
   - Закрытие всех сокетов
   - Освобождение ресурсов

---

## ВАЖНЫЕ МОМЕНТЫ

### Неблокирующий режим
- Все сокеты в неблокирующем режиме (O_NONBLOCK)
- `accept()`, `recv()`, `send()` не блокируют выполнение
- Если операция не может быть выполнена немедленно, возвращается EAGAIN/EWOULDBLOCK

### Event-driven архитектура
- Один поток обрабатывает множество подключений
- `poll()` мониторит все файловые дескрипторы одновременно
- События обрабатываются по мере поступления

### Управление ресурсами
- Каждое подключение = один файловый дескриптор
- Connection объекты автоматически закрывают fd в деструкторе
- Socket объекты автоматически закрывают fd в деструкторе

### Таймауты
- Каждое подключение отслеживает время последней активности
- Подключения без активности > 60 секунд закрываются
- Проверка происходит при каждом таймауте poll()

---

## Зависимости между компонентами

```
main.cpp
  ├─ использует ConfigParser для парсинга
  └─ использует Server для запуска сервера

Server
  ├─ использует ConfigParser (получает конфигурацию)
  ├─ создает и управляет Socket объектами
  └─ создает и управляет Connection объектами

Socket
  └─ независимый компонент (только системные вызовы)

Connection
  └─ независимый компонент (только хранит данные о клиенте)

ConfigParser
  └─ независимый компонент (только парсинг файлов)
```

---

## Резюме

1. **ConfigParser** парсит конфигурацию и создает структуры данных
2. **Server::init()** использует конфигурацию для создания Socket объектов
3. **Socket** создает слушающие сокеты через системные вызовы
4. **Server::run()** использует poll() для мониторинга событий
5. При новом подключении создается **Connection** объект
6. **Connection** хранит информацию о клиенте и управляется Server'ом
7. Все компоненты работают вместе в event-driven архитектуре
