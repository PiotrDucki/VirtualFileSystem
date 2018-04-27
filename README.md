# 1. Cel ćwiczenia

  Należy napisać w środowisku systemu Minix program w języku C (oraz skrypt
  demonstrujący wykorzystanie tego programu) realizujący podstawowe funkcje
  systemu plików.

2. Funkcje programu

  System plików należy zorganizować w dużym pliku o zadanej wielkości, który
  będzie "wirtualnym dyskiem". Program powinien tworzyć dysk wirtualny, oraz
  dokonywać zapisów i odczytów w celu zrealizowania podstawowych operacji na
  dysku, związanych z zarządzaniem katalogiem, alokacją plików oraz
  utrzymywaniem unikalności nazw.

3. Zadanie do zrealizowania

  W pliku na dysku należy zorganizować system plików z jednopoziomowym
  katalogiem.  Elementem katalogu jest opis pliku, zawierający co najmniej
  nazwę, wielkość i sposób rozmieszczenia pliku na wirtualnym dysku. Należy
  zaimplementować następujące operacje, dostępne dla użytkownika programu:

	- tworzenie wirtualnego dysku,

	- kopiowanie pliku z dysku systemu Minix na dysk wirtualny,

	- kopiowanie pliku z dysku wirtualnego na dysk systemu Minix,

	- wyświetlanie katalogu dysku wirtualnego,

	- usuwanie pliku z wirtualnego dysku,

	- usuwanie wirtualnego dysku,

	- wyświetlenie zestawienia z aktualną mapą zajętości wirtualnego dysku -
	  czyli listy kolejnych obszarów wirtualnego dysku z opisem: adres, typ
	  obszaru, rozmiar, stan (np. dla bloków danych: wolny/zajęty).

  Program ma kontrolować wielkość dostępnego miejsca na wirtualnym dysku i
  pojemność katalogu, reagować na próby przekroczenia tych wielkości.

  Nie trzeba realizować funkcji otwierania pliku ani czytania/pisania
  fragmentów pliku.

  Nie trzeba realizować funkcji związanych z współbieżnym dostępem. Zakłada
  się dostęp sekwencyjny i wyłączny do wirtualnego dysku.

  Należy przygotować demonstrację (zgrupowanie serii poleceń w postaci skryptu
  interpretera sh) prezentującą słabe i silne strony przyjętego rozwiązania w
  kontekście ewentualnych zewnętrznej i wewnętrznej fragmentacji.
