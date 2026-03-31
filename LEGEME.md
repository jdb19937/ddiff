# ddiff

Forma compacta differentiarum quae translationes truncorum inter plicas detegit.

Cum codex reficitur dividendo plicam magnum in modulos, `git diff`
totum plicam veterem ut deletum ostendit et omnes plicas novos ut
creatos — etiamsi maior pars codicis solum translata est. `ddiff` differentiam
unitam e flumine stdin legit, truncos translatos invenit, et eos separatim
a mutationibus veris refert.

> **Nota:** README.md ignorandum est. Anglice scriptum est ad barbaros ineruditos pecunia emungendos.

## Aedificatio

```
face
```

Vel directe:

```
cc -O2 -o ddiff ddiff.c
```

Nullae dependentiae praeter compilatorem linguae C et libc requiruntur.

## Usus

```
git diff main | ./ddiff
git diff HEAD~3 | ./ddiff
git diff abc123..def456 | ./ddiff
```

Quaelibet differentia unita e flumine stdin accepta est. Exitus structus
ad stdout scribitur. Summarium ad stderr.

## Forma exitus

Exitus tres partes habet.

### PLICAE

Omnes plicas in differentia cum operatione sua enumerat:

```
PLICAE
D iter/ad/plicam_deletum.py
C iter/ad/plicam_creatum.py
M iter/ad/plicam_mutatum.py
```

`D` = deletum, `C` = creatum, `M` = mutatum.

### TRANSLATIONES

Quisque truncus translatus. Caput fontem et destinationem, tractus
versuum, versus totales (`v` = versus), et numerum versuum dissimilium
intra truncum (`d` = dissimilia) ostendit:

```
TRANSLATIONES 120
T vetus/editors.py:[60,87] >> novus/editors/_common.py:[15,42] 28v 2d
  @3
  -def _emu_to_inches(v: int) -> float:
  +def emu_to_inches(v: int) -> float:
  @7
  -def _get_auto_shape_type(shape: Any) -> str | None:
  +def get_auto_shape_type(shape: Any) -> str | None:
.
T vetus/editors.py:[113,275] >> novus/editors/_common.py:[43,205] 163v 2d
  @103
  -def _ensure_shape_word_wrap(shape: Any) -> None:
  +def ensure_shape_word_wrap(shape: Any) -> None:
  @154
  -def _parse_hex_rgb(s: str) -> RGBColor:
  +def parse_hex_rgb(s: str) -> RGBColor:
.
```

Translatio exacta (0d) nullas lineas delta habet — solum caput `T` et `.`:

```
T vetus/editors.py:[88,112] >> novus/editors/charts.py:[24,48] 25v 0d
.
```

Signum `@N` distantiam intra truncum ubi dissimilia incipiunt indicat.
Versus praefixo `-` e fonte sunt; `+` ex destinatione.

### MUTATIONES

Omnia quae nulla translatione teguntur — versus vere novi vel vere deleti:

```
MUTATIONES
C iter/ad/novum/__init__.py
  +1 from .charts import insert_chart
  +2 from .formatting import apply_shape_format
.
M iter/ad/plicam_mutatum.py
  -15 import vetus.modulus as m
  +15 import novus.modulus as m
.
```

Quisque versus praefixum `-` vel `+` habet, sequente numero versus in
plica vetere vel novo respective. Punctum `.` finem sectionis cuiusque
plicae signat.

## dpatch — instrumentum applicationis

`dpatch` formam ddiff e flumine stdin legit et mutationes in plicas
locales applicat, statum novum e plicaes veteribus et ddiff
reconstruens.

### Aedificatio

```
face
```

Vel directe:

```
cc -O2 -o dpatch dpatch.c
```

### Usus

```
./ddiff < aliqua.diff | ./dpatch
```

Vel e plica ddiff servato:

```
./dpatch < servatum.ddiff
```

`dpatch` in directorio praesenti operatur. Fasciculos fontis in ddiff
nominatos legit, translationes et mutationes applicat, plicas creatos
et mutatos scribit, plicas deletos removet. Directoria parentalia
automatice creantur.

Nuntii status ad stderr scribuntur.

## Probationes

Probationes totum circuitum verificant: `diff -ruN` differentiam unitam
inter `probata/veterum/` (plicae veteres) et `probata/novum/` (plicae novi)
gignit, `ddiff` eam comprimit, `dpatch` in copiam plicarum veterum
applicat, et exitus cum plicaes novis exspectatis comparatur.

```
face proba
```

Probatio prospera `PROBATIO SVCCESSIT` scribit. Si fallit, `PROBATIO FALLIT`
cum differentiis inventis scribitur.

Gradus probationis `face proba`:

1. `probata/veterum/*` in directorium temporaneum `probata/applicatio/` copiantur.
2. `diff -ruN veterum novum` (intra `probata/`) per `./ddiff` ducitur.
3. Exitus ddiff per `./dpatch` (intra `probata/applicatio/`) ducitur.
4. `probata/novum/` cum `probata/applicatio/` per `diff -r` comparatur.
5. Directorium temporaneum removetur.

## Inversibilitas

Forma principio inversibilis est. Datis plicaes pristinis et ddiff:

1. Versus in fontibus TRANSLATIONUM e plicaes veteribus tolluntur et
   (deltis applicatis) in positiones destinationis plicarum novorum
   ponuntur.
2. Versus in MUTATIONIBUS cum `-` e plicaes veteribus tolluntur.
3. Versus in MUTATIONIBUS cum `+` plicaes novis adduntur.
4. Ceteri versus immutati manent.

## Algorithmus

1. **Resolutio**: differentia unita in versus sublatos et additos pro
   quoque plica resolvitur.
2. **Histogramma distantiarum**: pro quoque pare plicarum, quisque
   versus per sigillum notatur et distantiae `j − i` pro omnibus paribus
   concordantibus `(sublata[i], addita[j])` numerantur. Frequentia magna
   ad aliquam distantiam truncum magnum ad illam translationem indicat.
3. **Extractio truncorum**: pro quaque distantia significanti, tractus
   continui versuum concordantium inveniuntur (paucis lacunis pro
   mutationibus parvis intra truncos translatos toleratis).
4. **Electio avara**: candidati ordine magnitudinis decrescenti ordinantur,
   versus possidentur. Candidati partim conflicentes ad partes liberas
   secantur potius quam integri abiciuntur.
5. **Scriptio**: translationes cum deltis intra lineas, deinde reliqua
   non possessa ut mutationes.

## Limites

- Concordantia per sigillum exactum versus fit. Truncus translatus ubi
  plurimi versus parvas mutationes habent in multas translationes parvas
  frangitur vel omnino in mutationes cadit.
- Histogramma distantiarum truncos invenit qui ordinem internum versuum
  servant. Versus penitus permutati intra regionem translatam non
  detegentur.
- Fasciculi binarii ignorantur.

## Cancer

Translatio Rustica in `cancer/` iacet. Non curatur.
