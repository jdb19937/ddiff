/* =========================================================================
 *  DDIFF — INSTRUMENTUM DIFFERENTIARUM COMPACTARUM
 *
 *  Hoc instrumentum differentiam unitam (unified diff) legit e flumine
 *  stdin, translationes truncorum inter plicas detegit, et formam
 *  compactam "ddiff" ad stdout scribit.
 *
 *  Ubi multae functiones inter plicas moventur cum paucis mutationibus,
 *  haec forma multo brevior est quam differentia pristina.
 *
 *  Forma ddiff inversibilis est: ex statu pristino et ddiff, status novus
 *  reconstrui potest.
 *
 *  Compilatio:  cc -O2 -o ddiff ddiff.c
 *  Usus:        git diff ... | ./ddiff
 *               ddiff plica.diff
 *               ddiff -r vetus/ novus/
 *
 *  Anno MMXXVI
 *
 * =========================================================================
 *
 *  FORMA DDIFF — Versio I
 *
 *  ddiff versio I
 *
 *  PLICAE                              — index plicarum
 *  {D|C|M} <iter>                         — D=deletum C=creatum M=mutatum
 *
 *  TRANSLATIONES <n>                      — motus truncorum detecti
 *  T <fons>:[a,b] >> <dest>:[c,d] Nv Md  — truncus N versuum, M dissimilium
 *    @<distantia>                         — positio dissimilium intra truncum
 *    -<versus fontis>                     — versus e fonte
 *    +<versus destinationis>              — versus in destinatione
 *  .                                      — finis translationis
 *
 *  MUTATIONES                             — reliqua non translata
 *  {D|C|M} <iter>
 *    {-|+}<numerus> <argumentum>          — -N = sublatum ex linea N veteris
 *  .                                        +N = additum ad lineam N novi
 *
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- Constantiae universales ---------- */

#define LIMEN_TRUNCI         5   /* versus minimi pro translatione  */
#define LIMEN_LACUNAE        3   /* hiatus maximus intra truncum    */
#define LIMEN_FREQUENTIAE   30   /* versus nimis communes ignorantur */

/* ---------- Genera plicarum ---------- */

enum { MUTATUM = 0, DELETUM = 1, CREATUM = 2 };

/* ---------- Typi principales ---------- */

typedef struct {
    char     *argumentum;        /* textus versus (sine praefixo +/-)      */
    uint64_t  sigillum;          /* FNV-1a hash argumenti                  */
    int       numerus;           /* numerus versus in plica (1-index)  */
    int       translatum;        /* 1 si a translatione iam possessus      */
} Versus;

typedef struct {
    Versus *res;
    int     longitudo;
    int     capacitas;
} SeriesVersuum;

typedef struct {
    char          *iter_vetus;   /* iter pristinum (a/ latus)   */
    char          *iter_novus;   /* iter novum     (b/ latus)   */
    int            genus;        /* MUTATUM / DELETUM / CREATUM */
    SeriesVersuum  sublata;      /* versus sublati  (- lineae)  */
    SeriesVersuum  addita;       /* versus additi   (+ lineae)  */
} Plica;

typedef struct {
    int  index_fontis;           /* index plicae fontis           */
    int  index_destinationis;    /* index plicae destinationis    */
    int  initium_fontis;         /* primus index in sublata[] fontis */
    int  initium_dest;           /* primus index in addita[] dest    */
    int  magnitudo;              /* versus totales in trunco         */
    int  concordantes;           /* versus exacte concordantes       */
    int  numerus_fontis;         /* numerus versus in plica vet. */
    int  numerus_dest;           /* numerus versus in plica novo */
} Translatio;

typedef struct {
    uint64_t sigillum;
    int      index;
} ParSigilli;

/* ---------- Status globalis ---------- */

static Plica  *plicae;
static int          num_plic, cap_plic;

static Translatio  *candidati;
static int          num_cand, cap_cand;

static Translatio  *translationes;
static int          num_trans, cap_trans;

/* =========================================================================
 *  Functiones memoriae
 * ========================================================================= */

static void *para(size_t n)
{
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "ERROR: memoria exhausta\n"); exit(1); }
    return p;
}

static void *para_pura(size_t numerus, size_t magnitudo)
{
    void *p = calloc(numerus, magnitudo);
    if (!p) { fprintf(stderr, "ERROR: memoria exhausta\n"); exit(1); }
    return p;
}

static void *redimensiona(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (!q) { fprintf(stderr, "ERROR: memoria exhausta\n"); exit(1); }
    return q;
}

static char *duplica(const char *filum)
{
    size_t n = strlen(filum) + 1;
    char *d = para(n);
    memcpy(d, filum, n);
    return d;
}

/* =========================================================================
 *  Computatio sigilli — FNV-1a 64-bit
 * ========================================================================= */

static uint64_t computa_sigillum(const char *filum)
{
    uint64_t h = 14695981039346656037ULL;
    for (const unsigned char *p = (const unsigned char *)filum; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    return h;
}

/* =========================================================================
 *  Operationes serierum dynamicarum
 * ========================================================================= */

static void adde_versum(SeriesVersuum *sv, const char *argumentum, int numerus)
{
    if (sv->longitudo >= sv->capacitas) {
        sv->capacitas = sv->capacitas ? sv->capacitas * 2 : 256;
        sv->res = redimensiona(sv->res, (size_t)sv->capacitas * sizeof(Versus));
    }
    Versus *v = &sv->res[sv->longitudo++];
    v->argumentum  = duplica(argumentum);
    v->sigillum    = computa_sigillum(argumentum);
    v->numerus     = numerus;
    v->translatum  = 0;
}

static int adde_plicam(void)
{
    if (num_plic >= cap_plic) {
        cap_plic = cap_plic ? cap_plic * 2 : 32;
        plicae = redimensiona(plicae, (size_t)cap_plic * sizeof(Plica));
    }
    memset(&plicae[num_plic], 0, sizeof(Plica));
    return num_plic++;
}

static void adde_candidatum(const Translatio *t)
{
    if (num_cand >= cap_cand) {
        cap_cand = cap_cand ? cap_cand * 2 : 256;
        candidati = redimensiona(candidati, (size_t)cap_cand * sizeof(Translatio));
    }
    candidati[num_cand++] = *t;
}

static void adde_translationem(const Translatio *t)
{
    if (num_trans >= cap_trans) {
        cap_trans = cap_trans ? cap_trans * 2 : 256;
        translationes = redimensiona(translationes, (size_t)cap_trans * sizeof(Translatio));
    }
    translationes[num_trans++] = *t;
}

/* =========================================================================
 *  Extractio itineris e linea --- vel +++
 *
 *  Exscindit tempus (post tabulam) et primam partem itineris
 *  (a/, b/, vel nomen directorii).  /dev/null intactum servatur.
 * ========================================================================= */

static char *duplica_n(const char *filum, size_t n)
{
    char *d = para(n + 1);
    memcpy(d, filum, n);
    d[n] = '\0';
    return d;
}

static char *extrahe_iter(const char *p)
{
    /* Trunca ad tabulam (tempus in diff vulgari) */
    const char *tab = strchr(p, '\t');
    size_t lon = tab ? (size_t)(tab - p) : strlen(p);

    /* /dev/null intactum servatur */
    if (lon == 9 && strncmp(p, "/dev/null", 9) == 0)
        return duplica("/dev/null");

    /* Exscinde primam partem itineris (a/, b/, vel nomen directorii) */
    const char *sep = memchr(p, '/', lon);
    if (sep) {
        sep++;
        return duplica_n(sep, lon - (size_t)(sep - p));
    }
    return duplica_n(p, lon);
}

/* =========================================================================
 *  Resolutio differentiae ex stdin
 *
 *  Legit totum flumen, dividit in versus, resolvit in plicas
 *  cum suis sublatis et additis.
 * ========================================================================= */

static void resolve_differentiam(FILE *fons)
{
    /* I. Lege totum flumen in thesaurum */
    size_t cap_th = 1 << 16, lon_th = 0;
    char *thesaurus = para(cap_th);
    size_t lecta;
    while ((lecta = fread(thesaurus + lon_th, 1, cap_th - lon_th - 1, fons)) > 0) {
        lon_th += lecta;
        if (lon_th + 1 >= cap_th) {
            cap_th *= 2;
            thesaurus = redimensiona(thesaurus, cap_th);
        }
    }
    thesaurus[lon_th] = '\0';

    /* II. Divide in versus (mutando '\n' -> '\0' in loco) */
    int cap_v = 16384, num_v = 0;
    char **versus = para((size_t)cap_v * sizeof(char *));
    {
        char *p = thesaurus;
        while (*p) {
            if (num_v >= cap_v) {
                cap_v *= 2;
                versus = redimensiona(versus, (size_t)cap_v * sizeof(char *));
            }
            versus[num_v++] = p;
            while (*p && *p != '\n') p++;
            if (*p == '\n') { *p = '\0'; p++; }
        }
    }

    /* III. Resolve plicas, segmenta, versus sublatos et additos */
    int idx = -1;                   /* index plicae praesentis */
    int num_vet = 0, num_nov = 0;   /* numeri versuum currentium  */

    for (int i = 0; i < num_v; i++) {
        const char *lin = versus[i];

        /* Novus plica incipit (diff --git ... vel diff -ruN ...) */
        if (strncmp(lin, "diff ", 5) == 0) {
            idx = adde_plicam();
            continue;
        }
        if (idx < 0) continue;

        Plica *f = &plicae[idx];

        if (strncmp(lin, "deleted file", 12) == 0) {
            f->genus = DELETUM;
            continue;
        }
        if (strncmp(lin, "new file", 8) == 0) {
            f->genus = CREATUM;
            continue;
        }

        /* Iter pristinum */
        if (strncmp(lin, "--- ", 4) == 0) {
            f->iter_vetus = extrahe_iter(lin + 4);
            if (strcmp(f->iter_vetus, "/dev/null") == 0)
                f->genus = CREATUM;
            continue;
        }
        /* Iter novum */
        if (strncmp(lin, "+++ ", 4) == 0) {
            f->iter_novus = extrahe_iter(lin + 4);
            if (strcmp(f->iter_novus, "/dev/null") == 0)
                f->genus = DELETUM;
            continue;
        }

        /* Caput segmenti: @@ -a,b +c,d @@ */
        if (lin[0] == '@' && lin[1] == '@' && lin[2] == ' ') {
            const char *q = lin + 3;
            int s_vet = 0, n_vet = 1, s_nov = 0, n_nov = 1;
            if (*q == '-') q++;
            s_vet = (int)strtol(q, (char **)&q, 10);
            if (*q == ',') { q++; n_vet = (int)strtol(q, (char **)&q, 10); }
            while (*q == ' ') q++;
            if (*q == '+') q++;
            s_nov = (int)strtol(q, (char **)&q, 10);
            if (*q == ',') { q++; n_nov = (int)strtol(q, (char **)&q, 10); }
            num_vet = s_vet;
            num_nov = s_nov;
            /* -0,0 = plica vetus vacuus → creatum
             * +0,0 = plica novus vacuus → deletum */
            if (s_vet == 0 && n_vet == 0 && f->genus == MUTATUM)
                f->genus = CREATUM;
            if (s_nov == 0 && n_nov == 0 && f->genus == MUTATUM)
                f->genus = DELETUM;
            continue;
        }

        /* Lineae quas ignoramus */
        if (strncmp(lin, "index ",      6)  == 0 ||
            strncmp(lin, "similarity", 10)  == 0 ||
            strncmp(lin, "rename ",     7)  == 0 ||
            strncmp(lin, "old mode",    8)  == 0 ||
            strncmp(lin, "new mode",    8)  == 0 ||
            strncmp(lin, "Binary",      6)  == 0 ||
            lin[0] == '\\')
            continue;

        /* Versus contenti */
        if (lin[0] == '-') {
            adde_versum(&f->sublata, lin + 1, num_vet);
            num_vet++;
        } else if (lin[0] == '+') {
            adde_versum(&f->addita, lin + 1, num_nov);
            num_nov++;
        } else if (lin[0] == ' ') {
            num_vet++;
            num_nov++;
        }
    }

    /* Tutela: imple itinera vacua */
    for (int i = 0; i < num_plic; i++) {
        if (!plicae[i].iter_vetus)
            plicae[i].iter_vetus = duplica("/dev/null");
        if (!plicae[i].iter_novus)
            plicae[i].iter_novus = duplica("/dev/null");
    }

    free(versus);
    /* Argumenta versuum iam duplicata sunt; thesaurus liberari potest */
    free(thesaurus);
}

/* =========================================================================
 *  Investigatio translationum inter plicas
 *
 *  Methodus: pro quoque pare plicarum (fons, destinatio),
 *  histogramma distantiarum aedificatur. Pro quoque pare versuum
 *  concordantium (sublata[i], addita[j]), distantia d = j − i
 *  computatur. Distantiae frequentes truncos translatos indicant.
 *
 *  Truncos continuos extrahimus cum tolerantia lacunarum, deinde
 *  candidatos ordine magnitudinis decrescenti avare eligimus.
 * ========================================================================= */

static int compara_paria(const void *a, const void *b)
{
    const ParSigilli *pa = a, *pb = b;
    if (pa->sigillum < pb->sigillum) return -1;
    if (pa->sigillum > pb->sigillum) return  1;
    return 0;
}

/* Quaestio binaria: primus index ubi sigillum >= quaesitum */
static int quaere_primum(const ParSigilli *idx, int lon, uint64_t sig)
{
    int s = 0, d = lon;
    while (s < d) {
        int m = s + (d - s) / 2;
        if (idx[m].sigillum < sig) s = m + 1;
        else d = m;
    }
    return s;
}

/*
 * Invenit translationes inter plicam fontis (f_s)
 * et plicam destinationis (f_d).
 */
static void inveni_inter(int f_s, int f_d)
{
    Plica *fons = &plicae[f_s];
    Plica *dest = &plicae[f_d];
    int n = fons->sublata.longitudo;
    int m = dest->addita.longitudo;
    if (n == 0 || m == 0) return;

    /* I. Aedifica indicem ordinatum additorum secundum sigillum */
    ParSigilli *indicium = para((size_t)m * sizeof(ParSigilli));
    for (int j = 0; j < m; j++) {
        indicium[j].sigillum = dest->addita.res[j].sigillum;
        indicium[j].index    = j;
    }
    qsort(indicium, (size_t)m, sizeof(ParSigilli), compara_paria);

    /* II. Aedifica histogramma distantiarum (series plana) */
    int d_min = -(n - 1);
    int amplitudo = (m - 1) - d_min + 1;   /* = n + m - 1 */
    int *hist = para_pura((size_t)amplitudo, sizeof(int));

    for (int i = 0; i < n; i++) {
        uint64_t sig = fons->sublata.res[i].sigillum;
        int p = quaere_primum(indicium, m, sig);

        /* Numera concordantias in indice */
        int nconc = 0;
        for (int k = p; k < m && indicium[k].sigillum == sig; k++)
            nconc++;
        if (nconc == 0 || nconc > LIMEN_FREQUENTIAE)
            continue;

        /* Inscribe distantias in histogramma */
        for (int k = p; k < m && indicium[k].sigillum == sig; k++) {
            int dist = indicium[k].index - i;
            hist[dist - d_min]++;
        }
    }

    /* III. Pro quaque distantia significanti, extrahe truncos continuos */
    for (int di = 0; di < amplitudo; di++) {
        if (hist[di] < LIMEN_TRUNCI) continue;
        int dist = di + d_min;

        /* Limites validi indicis k in sublata:
         *   k >= 0,  k < n,  k+dist >= 0,  k+dist < m  */
        int k_min = 0;
        if (-dist > k_min) k_min = -dist;
        int k_max = n;
        if (m - dist < k_max) k_max = m - dist;
        if (k_max <= k_min) continue;

        /* Collige positiones concordantes */
        int cap_conc = 256, num_conc = 0;
        int *conc = para((size_t)cap_conc * sizeof(int));

        for (int k = k_min; k < k_max; k++) {
            if (fons->sublata.res[k].sigillum ==
                dest->addita.res[k + dist].sigillum)
            {
                if (num_conc >= cap_conc) {
                    cap_conc *= 2;
                    conc = redimensiona(conc, (size_t)cap_conc * sizeof(int));
                }
                conc[num_conc++] = k;
            }
        }

        /* Aggrega in truncos, tolerantia lacunarum LIMEN_LACUNAE */
        int cursor = 0;
        while (cursor < num_conc) {
            int initium_c = cursor;
            cursor++;
            while (cursor < num_conc &&
                   conc[cursor] - conc[cursor - 1] - 1 <= LIMEN_LACUNAE)
                cursor++;

            /* Reseca concordantias solitarias ab extremitatibus:
             * si ultima concordantia lacuna separata est ab antecedentibus,
             * eam removimus ne truncus ultra finem naturalem extendatur.
             * Similiter pro initio. */
            while (cursor > initium_c + 1 &&
                   conc[cursor - 1] - conc[cursor - 2] - 1 > 0)
                cursor--;
            while (initium_c + 1 < cursor &&
                   conc[initium_c + 1] - conc[initium_c] - 1 > 0)
                initium_c++;

            int num_in_trunco = cursor - initium_c;
            if (num_in_trunco < LIMEN_TRUNCI) continue;

            int k_init = conc[initium_c];
            int k_fin  = conc[cursor - 1];
            int magn   = k_fin - k_init + 1;

            Translatio t;
            t.index_fontis         = f_s;
            t.index_destinationis  = f_d;
            t.initium_fontis       = k_init;
            t.initium_dest         = k_init + dist;
            t.magnitudo            = magn;
            t.concordantes         = num_in_trunco;
            t.numerus_fontis       = fons->sublata.res[k_init].numerus;
            t.numerus_dest         = dest->addita.res[k_init + dist].numerus;
            adde_candidatum(&t);
        }

        free(conc);
    }

    free(hist);
    free(indicium);
}

/* Comparatio pro ordinatione candidatorum:
 * Primum secundum meritum netum (concordantes - dissimilia) decrescens,
 * deinde secundum concordantes decrescens.
 * Hoc praefert translationes puras (paucis dissimilibus) super impuras
 * eiusdem magnitudinis, impediens translationes transversas ubi functiones
 * corpora similia sed nomina diversa habent. */
static int compara_magnitudine(const void *a, const void *b)
{
    const Translatio *ta = a, *tb = b;
    int sa = 2 * ta->concordantes - ta->magnitudo;
    int sb = 2 * tb->concordantes - tb->magnitudo;
    if (sa != sb) return sb - sa;
    return tb->concordantes - ta->concordantes;
}

/*
 * Electio avara (greedy): candidatos ordine magnitudinis decrescenti
 * eligimus. Si candidatus partim cum translatione priore conflicit,
 * in partes liberas secamus potius quam totum abicimus.
 */
static void elige_translationes(void)
{
    qsort(candidati, (size_t)num_cand, sizeof(Translatio), compara_magnitudine);

    for (int i = 0; i < num_cand; i++) {
        Translatio *c  = &candidati[i];
        Plica *fs = &plicae[c->index_fontis];
        Plica *fd = &plicae[c->index_destinationis];

        /* Inveni partes liberas (non possessas) intra candidatum */
        int initium_partis = -1;
        for (int k = 0; k <= c->magnitudo; k++) {
            int occupatum = (k == c->magnitudo) ||
                fs->sublata.res[c->initium_fontis + k].translatum ||
                fd->addita.res[c->initium_dest   + k].translatum;

            if (!occupatum) {
                if (initium_partis < 0) initium_partis = k;
            } else if (initium_partis >= 0) {
                int lon_partis = k - initium_partis;
                if (lon_partis >= LIMEN_TRUNCI) {
                    /* Aedifica translationem ex parte libera */
                    Translatio pars = *c;
                    pars.initium_fontis += initium_partis;
                    pars.initium_dest   += initium_partis;
                    pars.magnitudo       = lon_partis;
                    pars.numerus_fontis  =
                        fs->sublata.res[pars.initium_fontis].numerus;
                    pars.numerus_dest    =
                        fd->addita.res[pars.initium_dest].numerus;
                    pars.concordantes = 0;
                    for (int j = 0; j < lon_partis; j++)
                        if (fs->sublata.res[pars.initium_fontis + j].sigillum ==
                            fd->addita.res[pars.initium_dest   + j].sigillum)
                            pars.concordantes++;

                    /* Posside versus */
                    for (int j = 0; j < lon_partis; j++) {
                        fs->sublata.res[pars.initium_fontis + j].translatum = 1;
                        fd->addita.res[pars.initium_dest   + j].translatum  = 1;
                    }
                    adde_translationem(&pars);
                }
                initium_partis = -1;
            }
        }
    }
}

/*
 * Invenit omnes translationes inter omnia paria plicarum.
 */
static void inveni_omnes(void)
{
    for (int s = 0; s < num_plic; s++)
        for (int d = 0; d < num_plic; d++)
            if (s != d)
                inveni_inter(s, d);
    elige_translationes();
}

/* =========================================================================
 *  Scriptio formae ddiff ad stdout
 * ========================================================================= */

/* Iter canonicum plicae (non "/dev/null") */
static const char *iter_plicae(const Plica *f)
{
    if (f->genus == DELETUM)
        return f->iter_vetus;
    return f->iter_novus;
}

/* Littera generis */
static char littera_generis(int genus)
{
    if (genus == DELETUM) return 'D';
    if (genus == CREATUM) return 'C';
    return 'M';
}

/* Comparatio pro ordinatione translationum: fons, deinde positio */
static int compara_ordine(const void *a, const void *b)
{
    const Translatio *ta = a, *tb = b;
    if (ta->index_fontis != tb->index_fontis)
        return ta->index_fontis - tb->index_fontis;
    return ta->initium_fontis - tb->initium_fontis;
}

static void scribe_ddiff(void)
{
    /* ---- Caput ---- */
    printf("ddiff versio I\n\n");

    /* ---- PLICAE ---- */
    printf("PLICAE\n");
    for (int i = 0; i < num_plic; i++)
        printf("%c %s\n", littera_generis(plicae[i].genus),
               iter_plicae(&plicae[i]));
    printf("\n");

    /* ---- TRANSLATIONES ---- */
    qsort(translationes, (size_t)num_trans, sizeof(Translatio), compara_ordine);
    printf("TRANSLATIONES %d\n", num_trans);

    for (int i = 0; i < num_trans; i++) {
        Translatio *t  = &translationes[i];
        Plica *fs = &plicae[t->index_fontis];
        Plica *fd = &plicae[t->index_destinationis];
        const char *it_f = fs->iter_vetus;
        const char *it_d = fd->iter_novus;
        int dissim = t->magnitudo - t->concordantes;
        int finis_fontis =
            fs->sublata.res[t->initium_fontis + t->magnitudo - 1].numerus;
        int finis_dest =
            fd->addita.res[t->initium_dest + t->magnitudo - 1].numerus;

        printf("T %s:[%d,%d] >> %s:[%d,%d] %dv %dd\n",
               it_f,
               t->numerus_fontis, finis_fontis,
               it_d,
               t->numerus_dest, finis_dest,
               t->magnitudo,
               dissim);

        /* Scribe delta dissimilium versuum */
        if (dissim > 0) {
            int k = 0;
            while (k < t->magnitudo) {
                /* Salta versus concordantes */
                while (k < t->magnitudo &&
                       fs->sublata.res[t->initium_fontis + k].sigillum ==
                       fd->addita.res[t->initium_dest   + k].sigillum)
                    k++;
                if (k >= t->magnitudo) break;

                /* Collige dissimilia continua */
                int g_init = k;
                while (k < t->magnitudo &&
                       fs->sublata.res[t->initium_fontis + k].sigillum !=
                       fd->addita.res[t->initium_dest   + k].sigillum)
                    k++;

                printf("  @%d\n", g_init);
                for (int g = g_init; g < k; g++)
                    printf("  -%s\n",
                           fs->sublata.res[t->initium_fontis + g].argumentum);
                for (int g = g_init; g < k; g++)
                    printf("  +%s\n",
                           fd->addita.res[t->initium_dest + g].argumentum);
            }
        }
        printf(".\n");
    }
    printf("\n");

    /* ---- MUTATIONES ---- */
    printf("MUTATIONES\n");
    for (int i = 0; i < num_plic; i++) {
        Plica *f = &plicae[i];
        int habet = 0;

        for (int j = 0; j < f->sublata.longitudo; j++) {
            if (!f->sublata.res[j].translatum) {
                if (!habet) {
                    printf("%c %s\n", littera_generis(f->genus),
                           iter_plicae(f));
                    habet = 1;
                }
                printf("  -%d %s\n", f->sublata.res[j].numerus,
                       f->sublata.res[j].argumentum);
            }
        }
        for (int j = 0; j < f->addita.longitudo; j++) {
            if (!f->addita.res[j].translatum) {
                if (!habet) {
                    printf("%c %s\n", littera_generis(f->genus),
                           iter_plicae(f));
                    habet = 1;
                }
                printf("  +%d %s\n", f->addita.res[j].numerus,
                       f->addita.res[j].argumentum);
            }
        }
        if (habet) printf(".\n");
    }

    /* ---- SUMMARIUM (ad stderr) ---- */
    int tot_trans = 0, tot_dissim = 0;
    for (int i = 0; i < num_trans; i++) {
        tot_trans  += translationes[i].magnitudo;
        tot_dissim += translationes[i].magnitudo - translationes[i].concordantes;
    }
    int rel_sub = 0, rel_add = 0;
    for (int i = 0; i < num_plic; i++) {
        Plica *f = &plicae[i];
        for (int j = 0; j < f->sublata.longitudo; j++)
            if (!f->sublata.res[j].translatum) rel_sub++;
        for (int j = 0; j < f->addita.longitudo; j++)
            if (!f->addita.res[j].translatum) rel_add++;
    }
    fprintf(stderr,
            "SUMMARIUM: %d plicae, %d translationes (%d versus, %d dissimilia)\n",
            num_plic, num_trans, tot_trans, tot_dissim);
    fprintf(stderr,
            "           reliqua: -%d +%d versus\n", rel_sub, rel_add);
}

/* =========================================================================
 *  Princeps
 * ========================================================================= */

static void auxilium_ddiff(void)
{
    printf("ddiff — forma compacta differentiarum\n\n");
    printf("usus:\n");
    printf("  ddiff                      legit differentiam unitam e stdin\n");
    printf("  ddiff <plica>              legit differentiam unitam e plica\n");
    printf("  ddiff -r <vetus> <novus>   comparat directoria et gignit ddiff\n\n");
    printf("optiones:\n");
    printf("  -h, --help                 hoc auxilium ostendit\n\n");
    printf("exitus structus ad stdout; summarium ad stderr.\n");
}

int main(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 ||
                      strcmp(argv[1], "--help") == 0)) {
        auxilium_ddiff();
        return 0;
    }

    if (argc == 4 && strcmp(argv[1], "-r") == 0) {
        char mandatum[4096];
        snprintf(mandatum, sizeof(mandatum),
                 "diff -ruN %s %s", argv[2], argv[3]);
        FILE *fons = popen(mandatum, "r");
        if (!fons) {
            fprintf(stderr, "ERROR: 'diff' excitari non potest\n");
            return 1;
        }
        resolve_differentiam(fons);
        pclose(fons);
        inveni_omnes();
        scribe_ddiff();
        return 0;
    }

    if (argc == 2) {
        FILE *fons = fopen(argv[1], "r");
        if (!fons) {
            fprintf(stderr, "ERROR: plica '%s' aperiri non potest\n", argv[1]);
            return 1;
        }
        resolve_differentiam(fons);
        fclose(fons);
        inveni_omnes();
        scribe_ddiff();
        return 0;
    }

    if (argc != 1) {
        fprintf(stderr, "ERROR: argumenta non intelleguntur — 'ddiff -h' vide\n");
        return 1;
    }

    resolve_differentiam(stdin);
    inveni_omnes();
    scribe_ddiff();
    return 0;
}
