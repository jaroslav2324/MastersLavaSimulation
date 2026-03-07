import csv
from collections import Counter


def find_duplicates(file_path: str, column_name: str) -> None:
    column_name = column_name.strip()

    with open(file_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        # Нормализуем заголовки (убираем пробелы)
        reader.fieldnames = [h.strip() for h in reader.fieldnames]

        if column_name not in reader.fieldnames:
            print(f"Колонка '{column_name}' не найдена.")
            print(f"Доступные колонки: {', '.join(reader.fieldnames)}")
            return

        values = [row[column_name].strip() for row in reader]

    counts = Counter(values)
    duplicates = {val: cnt for val, cnt in counts.items() if cnt > 1}

    if not duplicates:
        print(f"Дублирующихся значений в колонке '{column_name}' не найдено.")
        return

    print(f"Найдено дублирующихся значений: {len(duplicates)}\n")
    print(f"{'Значение':<20} {'Кол-во повторений'}")
    print("-" * 35)
    for val, cnt in sorted(duplicates.items(), key=lambda x: -x[1]):
        print(f"{val:<20} {cnt}")


if __name__ == "__main__":
    print("abiba")
    file_path = "1.csv"  # input("Путь к CSV файлу: ").strip()
    column_name = "int"  # input("Название колонки: ").strip()
    find_duplicates(file_path, column_name)
