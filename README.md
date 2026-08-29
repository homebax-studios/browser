1. Otevøi Zobrazit -> Terminal
2. cmake -S . -B build
3. cmake --build build

nebo:
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Debug