Axion Data Directory & Mathematischer Korpus
=============================================

Legen Sie die heruntergeladene und entpackte Datei 'research.sqlite' in diesen Ordner:
  H:\Axion\data\research.sqlite

(Alternativ kann die Datei auch direkt im Stammverzeichnis als 'H:\Axion\research.sqlite' abgelegt werden.)

Herkunft & Inhalt von 'research.sqlite':
- Basiert auf den von OpenAI veröffentlichten formalen mathematischen Forschungsunterlagen
  (OpenAI Formal Math Dataset / miniF2F im Namespace OAI.*) kombiniert mit Mathlib für Lean 4.
- Aufbereitet, normalisiert und indiziert durch Ralf Krümmel über die Axion-Ingestion-Pipeline.
- Beinhaltet 2.369.933 mathematische Deklarationen (966k def, 960k theorem, 420k lemma),
  149.204 relationale Modul-Abhängigkeiten und 48.266 Quell-Dateien.
- Streng schreibgeschützt (PRAGMA query_only = ON) als unantastbare Wahrheitsbasis.

Ausführliche Dokumentation: docs/DATASET_PROVENANCE.md
Download: https://github.com/kruemmel-python/Axion/releases/download/v1.0.0/Axion-Mathematical-Corpus-2.3M-research-db.zip

Axion Engine und die Axion Research IDE erkennen die Datenbank hier automatisch beim Start!
