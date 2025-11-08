# Code Formatting and Linting

This project uses `clang-format` for code formatting and `clang-tidy` for static analysis.

## Installation

### macOS
```bash
brew install clang-format
brew install llvm
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install clang-format clang-tidy
```

## Usage

### Format code

Format all source files:
```bash
make format
```

Format specific files manually:
```bash
clang-format -i src/main.cpp
clang-format -i include/Config.hpp
```

### Lint code

Run static analysis:
```bash
make lint
```

Run on specific files:
```bash
clang-tidy src/main.cpp -- -Wall -Wextra -Werror -std=c++98 -Iinclude
```

## Configuration

- `.clang-format` - formatting style configuration (tabs, 80 column limit, C++98 compatible)
- `.clang-tidy` - linting rules configuration

## IDE Integration

### VS Code
Install extensions:
- C/C++ (Microsoft)
- clang-format (xaver.clang-format)

Settings will be automatically picked up from `.clang-format`.

### CLion
- Settings → Editor → Code Style → Import Scheme → Import from `.clang-format`

### Vim/Neovim
Add to your `.vimrc`:
```vim
autocmd FileType cpp nnoremap <buffer><Leader>f :<C-u>ClangFormat<CR>
```

## Formatting Rules

The project uses:
- **Tabs** for indentation (not spaces)
- **80 characters** column limit
- **C++98** compatible style
- **Attach braces** style (opening braces on same line)
