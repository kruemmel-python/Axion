# Gemini Spectral Explorer: mathematisch korrigierte Forschungsvariante

**Installation:** Dieses ZIP direkt in `F:\MRE_Autonomous_Research_GeminiAPI` entpacken (Dateipfade beibehalten). Es erstellt ausschließlich `gemini-spectral-validated/` und `RUN_SPECTRAL_VALIDATION_WINDOWS.ps1` an der Projektwurzel. Der bestehende `src/`-Controller, Build, Datenbanken und `gemini-offline-review/` werden nicht überschrieben. Die Anwendung ist ein separates, offline ausführbares Forschungsunterprojekt; **die automatische Pipeline `software-research` wird dadurch nicht verändert**.

**Start unter Windows 11 / PowerShell:**

```powershell
Set-Location 'F:\MRE_Autonomous_Research_GeminiAPI'
.\RUN_SPECTRAL_VALIDATION_WINDOWS.ps1 -Scan
```

Alternativ manuell:

```powershell
& 'C:\Program Files\CMake\bin\cmake.exe' -S .\gemini-spectral-validated -B .\gemini-spectral-validated\build-windows -G 'Visual Studio 17 2022' -A x64
& 'C:\Program Files\CMake\bin\cmake.exe' --build .\gemini-spectral-validated\build-windows --config Release
.\gemini-spectral-validated\build-windows\Release\research_candidate.exe --selftest
.\gemini-spectral-validated\build-windows\Release\research_candidate.exe --scan
```

## Mathematische Korrektur

Der Graph ist ein ungerichteter Cayley-Graph von Z_n. Die Charaktere `exp(2*pi*i*k*x/n)` sind bekannte Eigenfunktionen seines Laplace-Operators. Statt einer numerisch beliebigen Jacobi-Eigenbasis wird die diskrete Fourier-Transformation verwendet. Alle Fourierenergien mit demselben Eigenwert werden vor der Entropieberechnung addiert. Das sind Normquadrate von Spektralprojektoren, also invariant gegenüber Rotation der Basis innerhalb entarteter Eigenräume.

Die kontinuierliche Quantenwanderung verwendet den Spektralpropagator `exp(-i*lambda*t)`. Die Rückkehrwahrscheinlichkeit wird aus Fourierenergien berechnet. Bei Delta-Signalen auf den Knoten eines zirkulanten Graphen sind Entropie und Rückkehrwahrscheinlichkeit **für alle Knoten identisch**, sofern die Anfangssignale von null verschieden sind. Die vorherige Zielwert-Rangfolge nach spektraler „Resonanz“ war daher methodisch irreführend. Der `legacy_score` wird ausschließlich als Vergleichskennzahl ausgegeben und ausdrücklich **nicht als neue mathematische Wirkung** interpretiert; die Scan-Rangfolge beruht transparent auf der Darstellungsanzahl (Gleichstände nach Zielwert aufgelöst).

**Achtung:** Die `representations`-Funktion zählt Wege mit Schritten aus der *symmetrisierten Menge verschiedener nichtnull modularer k-ter Potenzreste*. Sie zählt **nicht** direkt geordnete Darstellungen der Gleichung x^k+y^k+z^k=t über ganzen Zahlen.

Bei n=101, power=3, depth=3 ergibt das für Ziel 0 **9.900** und für alle Ziele 1..100 **9.901** Wege. Die früher geäußerte Aussage, alle 101 Werte hätten 9.901 Wege, war ungenau.

## Verifikation

Linux: `g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror` erfolgreich. `--selftest` bestanden. Getestet: Kongruenzpotenzen, Gesamtmasse der Wege, Eigenwert-Residual gegen direkte Laplace-Wirkung, DFT-Norm/Translation, Entartung und Zählwerte am vollständigen Graphen n=101. `--scan` ausgeführt.

Windows/MSVC: noch nicht live verifiziert; `.ps1` baut mit Visual Studio 2022. Keine Google-API oder Zugangsschlüssel erforderlich.

Grenzen: DFT ist direkt O(n²), Analyse aller Ziele kann bei größerem n aufwendig sein. Modulus ist auf 512, Tiefe auf 8 begrenzt; große Zählwerte verwenden double und können über dem ganzzahligen Exaktheitsbereich liegen. Die Eigenwertgruppierung verwendet eine relative Toleranz von 1e-9. Es wird **keine mathematische Entdeckung** behauptet.
