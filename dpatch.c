/* =========================================================================
 *  DPATCH — INSTRUMENTUM APPLICATIONIS DIFFERENTIARUM COMPACTARUM
 *
 *  Hoc instrumentum formam compactam "ddiff" legit e flumine stdin
 *  et mutationes in plicas locales applicat.
 *
 *  Datum ddiff et plicaes pristinis, plicas novos gignit:
 *  1. Versus in fontibus TRANSLATIONUM e plicaes veteribus tolluntur
 *     et (deltis applicatis) in positiones destinationis ponuntur.
 *  2. Versus in MUTATIONIBUS cum - e plicaes veteribus tolluntur.
 *  3. Versus in MUTATIONIBUS cum + plicaes novis adduntur.
 *  4. Ceteri versus immutati manent.
 *
 *  Compilatio:  cc -O2 -o dpatch dpatch.c
 *  Usus:        ./dpatch < exemplum.ddiff
 *
 *  Anno MMXXVI
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/* ---------- Genera plicarum ---------- */

enum { MUTATUM = 0, DELETUM = 1, CREATUM = 2 };

/* ---------- Typi principales ---------- */

typedef struct {
    char *iter;               /* iter plicae                      */
    int   genus;              /* DELETUM / CREATUM / MUTATUM         */
} Plica;

typedef struct {
    char  *iter_fontis;       /* iter plicae fontis               */
    int    initium_fontis;    /* numerus versus initialis in fonte   */
    int    finis_fontis;      /* numerus versus finalis in fonte     */
    char  *iter_dest;         /* iter plicae destinationis        */
    int    initium_dest;      /* numerus versus initialis in dest    */
    int    finis_dest;        /* numerus versus finalis in dest      */
    int    magnitudo;         /* versus totales (Nv)                 */
    int    dissimilia;        /* versus dissimiles (Md)              */
    char **substitutiones;    /* [0..magnitudo-1], NULL = identicum  */
} Translatio;

typedef struct {
    int   numerus;            /* numerus versus in plica         */
    int   additum;            /* 1 = additum (+), 0 = sublatum (-)  */
    char *argumentum;         /* textus versus                       */
} VersusOperatio;

typedef struct {
    char           *iter;     /* iter plicae                      */
    int             genus;    /* genus plicae                     */
    VersusOperatio *operationes;
    int             num_op;
    int             cap_op;
} MutatioPlicae;

typedef struct {
    char  *iter;              /* iter plicae in disco              */
    char **versus;            /* 1-index: versus[1..num_versus]       */
    int    num_versus;        /* numerus versuum                      */
} PlicaLecta;

typedef struct {
    int   numerus;            /* numerus versus destinationis         */
    char *argumentum;         /* textus versus                        */
} VersusAdditus;

/* ---------- Status globalis ---------- */

static Plica       *plicae;
static int               num_plic, cap_plic;

static Translatio       *translationes;
static int               num_trans, cap_trans;

static MutatioPlicae *mutat_plic;
static int               num_mut, cap_mut;

static PlicaLecta *lecti;         /* cache plicarum lectorum */
static int               num_lect, cap_lect;

static char            **versus_stdin;  /* versus ddiff ex stdin       */
static int               num_vs;

/* =========================================================================
 *  Functiones memoriae
 * ========================================================================= */

static void *para(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "ERROR: memoria exhausta\n");
        exit(1);
    }
    return p;
}

static void *para_pura(size_t numerus, size_t magnitudo)
{
    void *p = calloc(numerus, magnitudo);
    if (!p) {
        fprintf(stderr, "ERROR: memoria exhausta\n");
        exit(1);
    }
    return p;
}

static void *redimensiona(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (!q) {
        fprintf(stderr, "ERROR: memoria exhausta\n");
        exit(1);
    }
    return q;
}

static char *duplica(const char *filum)
{
    size_t n = strlen(filum) + 1;
    char *d  = para(n);
    memcpy(d, filum, n);
    return d;
}

static char *duplica_n(const char *filum, size_t n)
{
    char *d = para(n + 1);
    memcpy(d, filum, n);
    d[n] = '\0';
    return d;
}

/* =========================================================================
 *  Lectio fluminis stdin in versus
 * ========================================================================= */

static void lege_fons(FILE *fons)
{
    size_t cap_th   = 1 << 16, lon_th = 0;
    char *thesaurus = para(cap_th);
    size_t lecta;
    while (
        (
            lecta = fread(
                thesaurus + lon_th, 1, cap_th - lon_th - 1,
                fons
            )
        ) > 0
    ) {
        lon_th += lecta;
        if (lon_th + 1 >= cap_th) {
            cap_th *= 2;
            thesaurus = redimensiona(thesaurus, cap_th);
        }
    }
    thesaurus[lon_th] = '\0';

    int cap_v    = 4096;
    num_vs       = 0;
    versus_stdin = para((size_t)cap_v * sizeof(char *));

    char *p = thesaurus;
    while (*p) {
        if (num_vs >= cap_v) {
            cap_v *= 2;
            versus_stdin = redimensiona(
                versus_stdin,
                (size_t)cap_v * sizeof(char *)
            );
        }
        char *initium = p;
        while (*p && *p != '\n')
            p++;
        versus_stdin[num_vs++] = duplica_n(initium, (size_t)(p - initium));
        if (*p == '\n')
            p++;
    }
    free(thesaurus);
}

/* =========================================================================
 *  Lectio plicae e disco (cum cache)
 * ========================================================================= */

static char **lege_plicam(const char *iter, int *num_versus)
{
    FILE *f = fopen(iter, "r");
    if (!f) {
        fprintf(
            stderr, "ERROR: plica '%s' aperiri non potest: %s\n",
            iter, strerror(errno)
        );
        exit(1);
    }

    int cap       = 1024, num = 0;
    char **versus = para((size_t)(cap + 1) * sizeof(char *));
    versus[0]     = NULL;

    char linea[65536];
    while (fgets(linea, sizeof(linea), f)) {
        size_t lon = strlen(linea);
        if (lon > 0 && linea[lon - 1] == '\n')
            linea[--lon] = '\0';
        if (lon > 0 && linea[lon - 1] == '\r')
            linea[--lon] = '\0';
        num++;
        if (num >= cap) {
            cap *= 2;
            versus = redimensiona(
                versus,
                (size_t)(cap + 1) * sizeof(char *)
            );
        }
        versus[num] = duplica(linea);
    }

    fclose(f);
    *num_versus = num;
    return versus;
}

/* Legit plicam e disco aut e cache */
static char **cape_plicam(const char *iter, int *num_versus)
{
    /* Quaere in cache */
    for (int i = 0; i < num_lect; i++) {
        if (strcmp(lecti[i].iter, iter) == 0) {
            *num_versus = lecti[i].num_versus;
            return lecti[i].versus;
        }
    }

    /* Non inventum — lege e disco */
    int num;
    char **versus = lege_plicam(iter, &num);

    if (num_lect >= cap_lect) {
        cap_lect = cap_lect ? cap_lect * 2 : 16;
        lecti = redimensiona(
            lecti,
            (size_t)cap_lect * sizeof(PlicaLecta)
        );
    }
    lecti[num_lect].iter       = duplica(iter);
    lecti[num_lect].versus     = versus;
    lecti[num_lect].num_versus = num;
    num_lect++;

    *num_versus = num;
    return versus;
}

/* =========================================================================
 *  Resolutio formae ddiff ex stdin
 * ========================================================================= */

static int genus_ex_littera(char c)
{
    if (c == 'D')
        return DELETUM;
    if (c == 'C')
        return CREATUM;
    return MUTATUM;
}

static void resolve_ddiff(FILE *fons)
{
    lege_fons(fons);
    int i = 0;

    /* ---- I. Caput: "ddiff versio I" ---- */
    if (i >= num_vs || strcmp(versus_stdin[i], "ddiff versio I") != 0) {
        fprintf(stderr, "ERROR: caput 'ddiff versio I' deest\n");
        exit(1);
    }
    i++;
    while (i < num_vs && versus_stdin[i][0] == '\0')
        i++;

    /* ---- II. PLICAE ---- */
    if (i >= num_vs || strcmp(versus_stdin[i], "PLICAE") != 0) {
        fprintf(stderr, "ERROR: sectio PLICAE deest\n");
        exit(1);
    }
    i++;

    while (i < num_vs && versus_stdin[i][0] != '\0') {
        char *lin = versus_stdin[i];
        if (
            (lin[0] == 'D' || lin[0] == 'C' || lin[0] == 'M') &&
            lin[1] == ' '
        ) {
            if (num_plic >= cap_plic) {
                cap_plic = cap_plic ? cap_plic * 2 : 32;
                plicae = redimensiona(
                    plicae,
                    (size_t)cap_plic * sizeof(Plica)
                );
            }
            plicae[num_plic].genus = genus_ex_littera(lin[0]);
            plicae[num_plic].iter  = duplica(lin + 2);
            num_plic++;
        }
        i++;
    }
    while (i < num_vs && versus_stdin[i][0] == '\0') i++;

    /* ---- III. TRANSLATIONES ---- */
    if (i >= num_vs || strncmp(versus_stdin[i], "TRANSLATIONES", 13) != 0) {
        fprintf(stderr, "ERROR: sectio TRANSLATIONES deest\n");
        exit(1);
    }
    i++;

    while (i < num_vs) {
        char *lin = versus_stdin[i];
        if (lin[0] == '\0')
            break;
        if (strncmp(lin, "MUTATIONES", 10) == 0)
            break;

        if (lin[0] != 'T' || lin[1] != ' ') {
            i++;
            continue;
        }

        /* Resolve caput translationis:
         *   T iter_fontis:[a,b] >> iter_dest:[c,d] Nv Md           */
        const char *p    = lin + 2;
        const char *col1 = strstr(p, ":[");
        if (!col1) {
            i++;
            continue;
        }

        Translatio t;
        memset(&t, 0, sizeof(t));
        t.iter_fontis = duplica_n(p, (size_t)(col1 - p));

        const char *q    = col1 + 2;
        t.initium_fontis = (int)strtol(q, (char **)&q, 10);
        if (*q == ',')
            q++;
        t.finis_fontis = (int)strtol(q, (char **)&q, 10);

        const char *sep = strstr(q, " >> ");
        if (!sep) {
            free(t.iter_fontis);
            i++;
            continue;
        }
        const char *r    = sep + 4;
        const char *col2 = strstr(r, ":[");
        if (!col2) {
            free(t.iter_fontis);
            i++;
            continue;
        }

        t.iter_dest = duplica_n(r, (size_t)(col2 - r));

        const char *s  = col2 + 2;
        t.initium_dest = (int)strtol(s, (char **)&s, 10);
        if (*s == ',')
            s++;
        t.finis_dest = (int)strtol(s, (char **)&s, 10);

        while (*s && *s != ' ')
            s++;
        while (*s == ' ')
            s++;
        t.magnitudo = (int)strtol(s, (char **)&s, 10);
        if (*s == 'v')
            s++;
        while (*s == ' ')
            s++;
        t.dissimilia = (int)strtol(s, (char **)&s, 10);

        /* Crea seriem substitutionum (NULL = versus identicus) */
        t.substitutiones = para_pura((size_t)t.magnitudo, sizeof(char *));

        /* Lege versus delta usque ad punctum terminale */
        i++;
        int dist_cur = -1, plus_idx = 0;
        while (i < num_vs && strcmp(versus_stdin[i], ".") != 0) {
            lin = versus_stdin[i];
            if (strncmp(lin, "  @", 3) == 0) {
                dist_cur = (int)strtol(lin + 3, NULL, 10);
                plus_idx = 0;
            } else if (strncmp(lin, "  -", 3) == 0) {
                /* Versus fontis — non opus est, e plica legemus */
            } else if (strncmp(lin, "  +", 3) == 0) {
                if (
                    dist_cur >= 0 &&
                    dist_cur + plus_idx < t.magnitudo
                )
                    t.substitutiones[dist_cur + plus_idx] = duplica(lin + 3);
                plus_idx++;
            }
            i++;
        }
        if (i < num_vs)
            i++;   /* transili punctum "." */

        /* Serva translationem */
        if (num_trans >= cap_trans) {
            cap_trans = cap_trans ? cap_trans * 2 : 128;
            translationes = redimensiona(
                translationes,
                (size_t)cap_trans * sizeof(Translatio)
            );
        }
        translationes[num_trans++] = t;
        continue;   /* iam praeter punctum sumus */
    }
    while (i < num_vs && versus_stdin[i][0] == '\0') i++;

    /* ---- IV. MUTATIONES ---- */
    if (i >= num_vs || strncmp(versus_stdin[i], "MUTATIONES", 10) != 0)
        return;     /* nullae mutationes — licitum */
    i++;

    while (i < num_vs) {
        char *lin = versus_stdin[i];
        if (lin[0] == '\0') {
            i++;
            continue;
        }

        if (
            !(
                (lin[0] == 'D' || lin[0] == 'C' || lin[0] == 'M') &&
                lin[1] == ' '
            )
        ) {
            i++;
            continue;
        }

        /* Novus plica in mutationibus */
        if (num_mut >= cap_mut) {
            cap_mut = cap_mut ? cap_mut * 2 : 32;
            mutat_plic = redimensiona(
                mutat_plic,
                (size_t)cap_mut * sizeof(MutatioPlicae)
            );
        }
        MutatioPlicae *mf = &mutat_plic[num_mut];
        memset(mf, 0, sizeof(MutatioPlicae));
        mf->genus = genus_ex_littera(lin[0]);
        mf->iter  = duplica(lin + 2);
        num_mut++;

        i++;
        while (i < num_vs && strcmp(versus_stdin[i], ".") != 0) {
            lin = versus_stdin[i];
            if (
                strncmp(lin, "  -", 3) == 0 ||
                strncmp(lin, "  +", 3) == 0
            ) {
                if (mf->num_op >= mf->cap_op) {
                    mf->cap_op = mf->cap_op ? mf->cap_op * 2 : 64;
                    mf->operationes = redimensiona(
                        mf->operationes,
                        (size_t)mf->cap_op * sizeof(VersusOperatio)
                    );
                }
                VersusOperatio *op = &mf->operationes[mf->num_op];
                op->additum        = (lin[2] == '+') ? 1 : 0;
                const char *p      = lin + 3;
                op->numerus        = (int)strtol(p, (char **)&p, 10);
                if (*p == ' ')
                    p++;
                op->argumentum = duplica(p);
                mf->num_op++;
            }
            i++;
        }
        if (i < num_vs)
            i++;   /* transili punctum "." */
    }
}

/* =========================================================================
 *  Applicatio: aedificatio plicarum novorum
 * ========================================================================= */

static int compara_additos(const void *a, const void *b)
{
    return ((const VersusAdditus *)a)->numerus -
        ((const VersusAdditus *)b)->numerus;
}

/* Resolve argumentum destinationis pro indice k intra translationem */
static char *resolve_versum(
    const Translatio *t, int k,
    char **fons, int num_fontis
) {
    if (t->substitutiones[k])
        return duplica(t->substitutiones[k]);
    int lin = t->initium_fontis + k;
    if (lin >= 1 && lin <= num_fontis)
        return duplica(fons[lin]);
    return duplica("");
}

/* ---- Plica creatus: omnes versus ex translationibus et mutationibus ---- */

static char **aedifica_creatum(const char *iter, int *num_versus)
{
    int maximus = 0;

    for (int i = 0; i < num_trans; i++)
        if (
            strcmp(translationes[i].iter_dest, iter) == 0 &&
            translationes[i].finis_dest > maximus
        )
            maximus = translationes[i].finis_dest;

    for (int i = 0; i < num_mut; i++)
        if (strcmp(mutat_plic[i].iter, iter) == 0)
            for (int j = 0; j < mutat_plic[i].num_op; j++)
                if (
                    mutat_plic[i].operationes[j].additum &&
                    mutat_plic[i].operationes[j].numerus > maximus
                )
                    maximus = mutat_plic[i].operationes[j].numerus;

    if (maximus == 0) {
        *num_versus = 0;
        return NULL;
    }

    /* versus[1..maximus], versus[0] vacuus */
    char **versus = para_pura((size_t)(maximus + 1), sizeof(char *));

    /* Imple ex translationibus */
    for (int i = 0; i < num_trans; i++) {
        Translatio *t = &translationes[i];
        if (strcmp(t->iter_dest, iter) != 0)
            continue;

        int num_font;
        char **fons = cape_plicam(t->iter_fontis, &num_font);

        for (int k = 0; k < t->magnitudo; k++) {
            int lin_dest = t->initium_dest + k;
            if (lin_dest >= 1 && lin_dest <= maximus)
                versus[lin_dest] = resolve_versum(t, k, fons, num_font);
        }
    }

    /* Imple ex mutationibus */
    for (int i = 0; i < num_mut; i++) {
        if (strcmp(mutat_plic[i].iter, iter) != 0)
            continue;
        for (int j = 0; j < mutat_plic[i].num_op; j++) {
            VersusOperatio *op = &mutat_plic[i].operationes[j];
            if (op->additum && op->numerus >= 1 && op->numerus <= maximus) {
                free(versus[op->numerus]);
                versus[op->numerus] = duplica(op->argumentum);
            }
        }
    }

    *num_versus = maximus;
    return versus;
}

/* ---- Plica mutatus: veteres versus emendantur ---- */

static char **aedifica_mutatum(const char *iter, int *num_versus)
{
    /* I. Lege plicam veterem */
    int num_vet;
    char **veteres = cape_plicam(iter, &num_vet);

    /* II. Nota versus sublatos (ex translationibus et mutationibus) */
    int *sublatum = para_pura((size_t)(num_vet + 1), sizeof(int));

    for (int i = 0; i < num_trans; i++) {
        Translatio *t = &translationes[i];
        if (strcmp(t->iter_fontis, iter) != 0)
            continue;
        for (int k = t->initium_fontis; k <= t->finis_fontis; k++)
            if (k >= 1 && k <= num_vet)
                sublatum[k] = 1;
    }

    for (int i = 0; i < num_mut; i++) {
        if (strcmp(mutat_plic[i].iter, iter) != 0)
            continue;
        for (int j = 0; j < mutat_plic[i].num_op; j++) {
            VersusOperatio *op = &mutat_plic[i].operationes[j];
            if (!op->additum && op->numerus >= 1 && op->numerus <= num_vet)
                sublatum[op->numerus] = 1;
        }
    }

    /* III. Collige versus additos (ex translationibus et mutationibus) */
    int cap_add = 256, num_add = 0;
    VersusAdditus *addita = para((size_t)cap_add * sizeof(VersusAdditus));

    for (int i = 0; i < num_trans; i++) {
        Translatio *t = &translationes[i];
        if (strcmp(t->iter_dest, iter) != 0)
            continue;

        int num_font;
        char **fons = cape_plicam(t->iter_fontis, &num_font);

        for (int k = 0; k < t->magnitudo; k++) {
            if (num_add >= cap_add) {
                cap_add *= 2;
                addita = redimensiona(
                    addita,
                    (size_t)cap_add * sizeof(VersusAdditus)
                );
            }
            addita[num_add].numerus    = t->initium_dest + k;
            addita[num_add].argumentum = resolve_versum(
                t, k, fons,
                num_font
            );
            num_add++;
        }
    }

    for (int i = 0; i < num_mut; i++) {
        if (strcmp(mutat_plic[i].iter, iter) != 0)
            continue;
        for (int j = 0; j < mutat_plic[i].num_op; j++) {
            VersusOperatio *op = &mutat_plic[i].operationes[j];
            if (!op->additum)
                continue;
            if (num_add >= cap_add) {
                cap_add *= 2;
                addita = redimensiona(
                    addita,
                    (size_t)cap_add * sizeof(VersusAdditus)
                );
            }
            addita[num_add].numerus    = op->numerus;
            addita[num_add].argumentum = duplica(op->argumentum);
            num_add++;
        }
    }

    qsort(addita, (size_t)num_add, sizeof(VersusAdditus), compara_additos);

    /* IV. Computa longitudinem novam */
    int num_sublata = 0;
    for (int k = 1; k <= num_vet; k++)
        if (sublatum[k])
            num_sublata++;
    int num_nov = num_vet - num_sublata + num_add;

    /* V. Aedifica plicam novum duobus cursoribus */
    char **novi = para((size_t)(num_nov + 1) * sizeof(char *));
    novi[0]     = NULL;

    int cursor_vet = 1;     /* cursor in plica vetere  */
    int cursor_nov = 1;     /* cursor in plica novo    */
    int idx_add    = 0;     /* index in additis            */

    while (cursor_nov <= num_nov) {
        if (
            idx_add < num_add &&
            addita[idx_add].numerus == cursor_nov
        ) {
            /* Versus additus ex translatione vel mutatione */
            novi[cursor_nov] = addita[idx_add].argumentum;
            idx_add++;
        } else {
            /* Versus immutatus ex plica vetere */
            while (cursor_vet <= num_vet && sublatum[cursor_vet])
                cursor_vet++;
            if (cursor_vet <= num_vet)
                novi[cursor_nov] = duplica(veteres[cursor_vet++]);
            else
                novi[cursor_nov] = duplica("");
        }
        cursor_nov++;
    }

    free(sublatum);
    free(addita);
    *num_versus = num_nov;
    return novi;
}

/* =========================================================================
 *  Scriptio et deletio plicarum
 * ========================================================================= */

/* Creat directoria parentalia (similis mkdir -p) */
static void crea_directoria(const char *iter)
{
    char *copia = duplica(iter);
    for (char *p = copia + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(copia, 0755);
            *p = '/';
        }
    }
    free(copia);
}

static void scribe_plicam(const char *iter, char **versus, int num_versus)
{
    crea_directoria(iter);
    FILE *f = fopen(iter, "w");
    if (!f) {
        fprintf(
            stderr, "ERROR: plica '%s' scribi non potest: %s\n",
            iter, strerror(errno)
        );
        exit(1);
    }
    for (int i = 1; i <= num_versus; i++) {
        if (versus[i])
            fprintf(f, "%s\n", versus[i]);
        else
            fputc('\n', f);    /* lacuna — non debet accidere */
    }
    fclose(f);
}

/* =========================================================================
 *  Applicatio principalis
 * ========================================================================= */

static void applica(void)
{
    fprintf(
        stderr, "APPLICATIO: %d plicae, %d translationes\n",
        num_plic, num_trans
    );

    /* Phase I: aedifica argumenta nova in memoria */
    char ***argumenta    = para_pura((size_t)num_plic, sizeof(char **));
    int    *longitudines = para_pura((size_t)num_plic, sizeof(int));

    for (int i = 0; i < num_plic; i++) {
        if (plicae[i].genus == CREATUM)
            argumenta[i] = aedifica_creatum(
                plicae[i].iter,
                &longitudines[i]
            );
        else if (plicae[i].genus == MUTATUM)
            argumenta[i] = aedifica_mutatum(
                plicae[i].iter,
                &longitudines[i]
            );
    }

    /* Phase II: scribe plicas creatos et mutatos */
    int num_cre = 0, num_mut_n = 0, num_del = 0;

    for (int i = 0; i < num_plic; i++) {
        if (plicae[i].genus == CREATUM && argumenta[i]) {
            scribe_plicam(
                plicae[i].iter,
                argumenta[i], longitudines[i]
            );
            fprintf(
                stderr, "  C %s (%d versus)\n",
                plicae[i].iter, longitudines[i]
            );
            num_cre++;
        } else if (plicae[i].genus == MUTATUM && argumenta[i]) {
            scribe_plicam(
                plicae[i].iter,
                argumenta[i], longitudines[i]
            );
            fprintf(
                stderr, "  M %s (%d versus)\n",
                plicae[i].iter, longitudines[i]
            );
            num_mut_n++;
        }
    }

    /* Phase III: dele plicas (post scriptiones, ne fontes perdantur) */
    for (int i = 0; i < num_plic; i++) {
        if (plicae[i].genus != DELETUM)
            continue;
        if (remove(plicae[i].iter) == 0) {
            fprintf(stderr, "  D %s\n", plicae[i].iter);
            num_del++;
        } else if (errno != ENOENT) {
            fprintf(
                stderr, "MONITUM: plica '%s' deleri non potest: %s\n",
                plicae[i].iter, strerror(errno)
            );
        }
    }

    fprintf(
        stderr, "FACTUM: %d creati, %d mutati, %d deleti\n",
        num_cre, num_mut_n, num_del
    );

    free(argumenta);
    free(longitudines);
}

/* =========================================================================
 *  Princeps
 * ========================================================================= */

static void auxilium_dpatch(void)
{
    printf("dpatch — applicatio differentiarum compactarum\n\n");
    printf("usus:\n");
    printf("  dpatch                     legit ddiff e stdin, applicat in loco praesenti\n");
    printf("  dpatch <plica>             legit ddiff e plica, applicat in loco praesenti\n\n");
    printf("optiones:\n");
    printf("  -h, --help                 hoc auxilium ostendit\n");
}

int main(int argc, char **argv)
{
    if (
        argc == 2 && (
            strcmp(argv[1], "-h") == 0 ||
            strcmp(argv[1], "--help") == 0
        )
    ) {
        auxilium_dpatch();
        return 0;
    }

    if (argc == 2) {
        FILE *fons = fopen(argv[1], "r");
        if (!fons) {
            fprintf(stderr, "ERROR: plica '%s' aperiri non potest\n", argv[1]);
            return 1;
        }
        resolve_ddiff(fons);
        fclose(fons);
        applica();
        return 0;
    }

    if (argc != 1) {
        fprintf(stderr, "ERROR: argumenta non intelleguntur — 'dpatch -h' vide\n");
        return 1;
    }

    resolve_ddiff(stdin);
    applica();
    return 0;
}
