# Axion – Research IDE Benutzerhandbuch

Die **Axion Research IDE** (`axion_ide.exe`) ist das grafische Leitwerk der Axion-Plattform.

---

## 1. Starten der IDE

Die IDE kann direkt über das Starter-Skript im Hauptverzeichnis gestartet werden:
```cmd
START_AXION_IDE.cmd
```
Oder direkt aus dem Build-Verzeichnis:
```powershell
.\KR_BUILD\Release\axion_ide.exe
```

---

## 2. Benutzeroberfläche & Arbeitsbereiche

Die IDE ist in 7 spezialisierte Arbeitsbereiche (Tabs) unterteilt:

1. **K4D Research Lab**:
   * Auswahl des Kettenmodus: *Einzelne Zyklen*, *Lineare Forschungskette* oder *Verzweigtes Beweisnetz (Baum)*.
   * Strategiewahl: *Alle 6 Strategien*, *type_synthesis*, *systematic*, *random*, *cross_domain*, *mutation*, *iterative*.
   * LLM-Provider-Auswahl: *LM Studio (Lokal)* oder *Google Gemini (Cloud)*.
2. **Quelldatenbank**:
   * Durchsuchen, Filtern und Prüfen von extrahierten mathematischen Definitionen.
3. **C++ Algorithmen**:
   * Übersicht über generierte, lauffähige Kernel und deren Performancemetriken.
4. **C++ Editor + Test**:
   * Integrierter Code-Editor mit Syntaxprüfung und Ein-Klick-Kompilierung.
5. **Forschungsläufe**:
   * Chronologisches Protokoll aller durchgeführten Validierungszyklen.
6. **Experimentanalyse**:
   * Statistische Auswertung, Konvergenzmessungen und Phasenraumdiagramme (`Zustandsdiagramm`).
7. **Autonome Evolution & Meta-Fusion**:
   * Evolutionsläufe über mathematische Familien (Dauerlauf, Generationen-Rollback).
   * **Meta-Fusion**: Verschmelzung zweier Algorithmen-Familien (z. B. Tensor Attention & RoPE) zu einer neuen Hybrid-Architektur.

---

## 3. Menüleiste & Info-Dialog

Über den Menüpunkt **`Info`** in der Menüleiste der IDE erhalten Sie jederzeit vollständige Transparenz:

* **Über Axion...**: Zeigt die Versionsinformationen, die Architektur und den Entwickler **Ralf Krümmel**.
* **Mathematische Algorithmen & Herkunft...**: Detaillierte Offenlegung der mathematischen Methodik (Lean 4 Beweis-Engine, K4D Strategien, spektrale Laplace-Konsistenzprüfung, Orakel-Falsifikation).

---

## 4. Modernes Design & Farbgebung

Die Axion IDE verwendet eine zeitgemäße Slate-Dark-Optik:
* **Dunkle Windows-Titelleiste:** Automatische Einbindung des immersiven Dark-Modes (DWM).
* **Kontrastreiche Code- und Listenansichten:** Optimiert für ermüdungsfreies Arbeiten bei langen Forschungssitzungen.
