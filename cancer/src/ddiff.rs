/* =========================================================================
 *  DDIFF — INSTRUMENTUM DIFFERENTIARUM COMPACTARUM
 *
 *  Hoc instrumentum differentiam unitam (unified diff) legit e flumine
 *  stdin, translationes truncorum inter plicas detegit, et formam
 *  compactam "ddiff" ad stdout scribit.
 *
 *  Translatio fidelis ex lingua C in linguam Rust.
 *
 *  Compilatio:  cargo build --release --bin ddiff
 *  Usus:        git diff ... | ./ddiff
 *
 *  Anno MMXXVI
 * ========================================================================= */

use std::io::{self, Read, Write};

/* ---------- Constantiae universales ---------- */

const LIMEN_TRUNCI: i32 = 5; /* versus minimi pro translatione  */
const LIMEN_LACUNAE: i32 = 3; /* hiatus maximus intra truncum    */
const LIMEN_FREQUENTIAE: i32 = 30; /* versus nimis communes ignorantur */

/* ---------- Genera plicarum ---------- */

const MUTATUM: i32 = 0;
const DELETUM: i32 = 1;
const CREATUM: i32 = 2;

/* ---------- Typi principales ---------- */

struct Versus {
    argumentum: String, /* textus versus (sine praefixo +/-)      */
    sigillum: u64,      /* FNV-1a sigillum argumenti               */
    numerus: i32,       /* numerus versus in plica (1-index)   */
    translatum: bool,   /* verum si a translatione iam possessus   */
}

struct SeriesVersuum {
    res: Vec<Versus>,
}

struct Plica {
    iter_vetus: String,     /* iter pristinum (a/ latus)   */
    iter_novus: String,     /* iter novum     (b/ latus)   */
    genus: i32,             /* MUTATUM / DELETUM / CREATUM */
    sublata: SeriesVersuum, /* versus sublati  (- lineae)  */
    addita: SeriesVersuum,  /* versus additi   (+ lineae)  */
}

#[derive(Clone)]
struct Translatio {
    index_fontis: usize,        /* index plicae fontis           */
    index_destinationis: usize, /* index plicae destinationis    */
    initium_fontis: usize,      /* primus index in sublata[] fontis */
    initium_dest: usize,        /* primus index in addita[] dest    */
    magnitudo: usize,           /* versus totales in trunco         */
    concordantes: usize,        /* versus exacte concordantes       */
    numerus_fontis: i32,        /* numerus versus in plica vet. */
    numerus_dest: i32,          /* numerus versus in plica novo */
}

struct ParSigilli {
    sigillum: u64,
    index: usize,
}

/* =========================================================================
 *  Computatio sigilli — FNV-1a 64-bit
 * ========================================================================= */

fn computa_sigillum(filum: &str) -> u64 {
    let mut h: u64 = 14695981039346656037;
    for octetus in filum.bytes() {
        h ^= octetus as u64;
        h = h.wrapping_mul(1099511628211);
    }
    h
}

/* =========================================================================
 *  Operationes serierum dynamicarum
 * ========================================================================= */

fn adde_versum(series: &mut SeriesVersuum, argumentum: &str, numerus: i32) {
    let sigillum = computa_sigillum(argumentum);
    series.res.push(Versus {
        argumentum: argumentum.to_string(),
        sigillum,
        numerus,
        translatum: false,
    });
}

/* =========================================================================
 *  Extractio itineris e linea --- vel +++
 *
 *  Exscindit tempus (post tabulam) et primam partem itineris
 *  (a/, b/, vel nomen directorii).  /dev/null intactum servatur.
 * ========================================================================= */

fn extrahe_iter(p: &str) -> String {
    /* Trunca ad tabulam (tempus in diff vulgari) */
    let truncatum = match p.find('\t') {
        Some(pos) => &p[..pos],
        None => p,
    };

    /* /dev/null intactum servatur */
    if truncatum == "/dev/null" {
        return "/dev/null".to_string();
    }

    /* Exscinde primam partem itineris (a/, b/, vel nomen directorii) */
    match truncatum.find('/') {
        Some(pos) => truncatum[pos + 1..].to_string(),
        None => truncatum.to_string(),
    }
}

/* =========================================================================
 *  Resolutio differentiae ex stdin
 * ========================================================================= */

fn resolve_differentiam(plicae: &mut Vec<Plica>) {
    /* I. Lege totum flumen stdin in thesaurum */
    let mut thesaurus = String::new();
    io::stdin()
        .read_to_string(&mut thesaurus)
        .expect("ERROR: stdin legi non potest");

    /* II. Divide in versus */
    let versus: Vec<&str> = thesaurus.lines().collect();

    /* III. Resolve plicas, segmenta, versus sublatos et additos */
    let mut idx: Option<usize> = None;
    let mut num_vet: i32 = 0;
    let mut num_nov: i32 = 0;

    for lin in &versus {
        /* Novus plica incipit */
        if lin.starts_with("diff ") {
            plicae.push(Plica {
                iter_vetus: String::new(),
                iter_novus: String::new(),
                genus: MUTATUM,
                sublata: SeriesVersuum { res: Vec::new() },
                addita: SeriesVersuum { res: Vec::new() },
            });
            idx = Some(plicae.len() - 1);
            continue;
        }
        let fi = match idx {
            Some(i) => i,
            None => continue,
        };

        if lin.starts_with("deleted file") {
            plicae[fi].genus = DELETUM;
            continue;
        }
        if lin.starts_with("new file") {
            plicae[fi].genus = CREATUM;
            continue;
        }

        /* Iter pristinum */
        if lin.starts_with("--- ") {
            let iter = extrahe_iter(&lin[4..]);
            if iter == "/dev/null" {
                plicae[fi].genus = CREATUM;
            }
            plicae[fi].iter_vetus = iter;
            continue;
        }
        /* Iter novum */
        if lin.starts_with("+++ ") {
            let iter = extrahe_iter(&lin[4..]);
            if iter == "/dev/null" {
                plicae[fi].genus = DELETUM;
            }
            plicae[fi].iter_novus = iter;
            continue;
        }

        /* Caput segmenti: @@ -a,b +c,d @@ */
        if lin.starts_with("@@ ") {
            let q = &lin[3..];
            let (s_vet, n_vet, s_nov, n_nov) = resolve_caput_segmenti(q);
            num_vet = s_vet;
            num_nov = s_nov;
            if s_vet == 0 && n_vet == 0 && plicae[fi].genus == MUTATUM {
                plicae[fi].genus = CREATUM;
            }
            if s_nov == 0 && n_nov == 0 && plicae[fi].genus == MUTATUM {
                plicae[fi].genus = DELETUM;
            }
            continue;
        }

        /* Lineae quas ignoramus */
        if lin.starts_with("index ")
            || lin.starts_with("similarity")
            || lin.starts_with("rename ")
            || lin.starts_with("old mode")
            || lin.starts_with("new mode")
            || lin.starts_with("Binary")
            || lin.starts_with('\\')
        {
            continue;
        }

        /* Versus contenti */
        if lin.starts_with('-') {
            adde_versum(&mut plicae[fi].sublata, &lin[1..], num_vet);
            num_vet += 1;
        } else if lin.starts_with('+') {
            adde_versum(&mut plicae[fi].addita, &lin[1..], num_nov);
            num_nov += 1;
        } else if lin.starts_with(' ') {
            num_vet += 1;
            num_nov += 1;
        }
    }

    /* Tutela: imple itinera vacua */
    for f in plicae.iter_mut() {
        if f.iter_vetus.is_empty() {
            f.iter_vetus = "/dev/null".to_string();
        }
        if f.iter_novus.is_empty() {
            f.iter_novus = "/dev/null".to_string();
        }
    }
}

/* Resolutio capitis segmenti: -a,b +c,d */
fn resolve_caput_segmenti(versus: &str) -> (i32, i32, i32, i32) {
    let s_vet: i32;
    let mut n_vet: i32 = 1;
    let s_nov: i32;
    let mut n_nov: i32 = 1;

    let mut pos = 0;

    /* Transili signum '-' */
    while pos < versus.len() && versus.as_bytes()[pos] == b'-' {
        pos += 1;
    }

    /* Lege s_vet */
    let (s, p) = lege_numerum(versus, pos);
    s_vet = s;
    pos = p;
    if pos < versus.len() && versus.as_bytes()[pos] == b',' {
        pos += 1;
        let (n, p2) = lege_numerum(versus, pos);
        n_vet = n;
        pos = p2;
    }

    /* Transili spatia */
    while pos < versus.len() && versus.as_bytes()[pos] == b' ' {
        pos += 1;
    }
    /* Transili signum '+' */
    if pos < versus.len() && versus.as_bytes()[pos] == b'+' {
        pos += 1;
    }

    /* Lege s_nov */
    let (s2, p3) = lege_numerum(versus, pos);
    s_nov = s2;
    pos = p3;
    if pos < versus.len() && versus.as_bytes()[pos] == b',' {
        pos += 1;
        let (n2, _p4) = lege_numerum(versus, pos);
        n_nov = n2;
    }

    (s_vet, n_vet, s_nov, n_nov)
}

fn lege_numerum(filum: &str, initium: usize) -> (i32, usize) {
    let octeti = filum.as_bytes();
    let mut pos = initium;
    let mut valor: i32 = 0;
    while pos < octeti.len() && octeti[pos] >= b'0' && octeti[pos] <= b'9' {
        valor = valor * 10 + (octeti[pos] - b'0') as i32;
        pos += 1;
    }
    (valor, pos)
}

/* =========================================================================
 *  Investigatio translationum inter plicas
 * ========================================================================= */

fn inveni_inter(
    plicae: &mut Vec<Plica>,
    candidati: &mut Vec<Translatio>,
    f_s: usize,
    f_d: usize,
) {
    let n = plicae[f_s].sublata.res.len();
    let m = plicae[f_d].addita.res.len();
    if n == 0 || m == 0 {
        return;
    }

    /* I. Aedifica indicem ordinatum additorum secundum sigillum */
    let mut indicium: Vec<ParSigilli> = Vec::with_capacity(m);
    for j in 0..m {
        indicium.push(ParSigilli {
            sigillum: plicae[f_d].addita.res[j].sigillum,
            index: j,
        });
    }
    indicium.sort_by(|a, b| a.sigillum.cmp(&b.sigillum));

    /* II. Aedifica histogramma distantiarum */
    let d_min: i64 = -(n as i64 - 1);
    let amplitudo: usize = (m as i64 - 1 - d_min + 1) as usize; /* = n + m - 1 */
    let mut histogramma: Vec<i32> = vec![0; amplitudo];

    for i in 0..n {
        let sig = plicae[f_s].sublata.res[i].sigillum;

        /* Quaestio binaria: primus index ubi sigillum >= sig */
        let p = indicium.partition_point(|ps| ps.sigillum < sig);

        /* Numera concordantias */
        let mut num_conc = 0;
        let mut k = p;
        while k < m && indicium[k].sigillum == sig {
            num_conc += 1;
            k += 1;
        }
        if num_conc == 0 || num_conc > LIMEN_FREQUENTIAE as usize {
            continue;
        }

        /* Inscribe distantias in histogramma */
        let mut k = p;
        while k < m && indicium[k].sigillum == sig {
            let dist = indicium[k].index as i64 - i as i64;
            histogramma[(dist - d_min) as usize] += 1;
            k += 1;
        }
    }

    /* III. Pro quaque distantia significanti, extrahe truncos continuos */
    for di in 0..amplitudo {
        if histogramma[di] < LIMEN_TRUNCI {
            continue;
        }
        let dist = di as i64 + d_min;

        /* Limites validi */
        let mut k_min: i64 = 0;
        if -dist > k_min {
            k_min = -dist;
        }
        let mut k_max: i64 = n as i64;
        if (m as i64 - dist) < k_max {
            k_max = m as i64 - dist;
        }
        if k_max <= k_min {
            continue;
        }

        /* Collige positiones concordantes */
        let mut conc: Vec<usize> = Vec::new();
        for k in (k_min as usize)..(k_max as usize) {
            let j = (k as i64 + dist) as usize;
            if plicae[f_s].sublata.res[k].sigillum == plicae[f_d].addita.res[j].sigillum {
                conc.push(k);
            }
        }

        /* Aggrega in truncos, tolerantia lacunarum LIMEN_LACUNAE */
        let mut cursor = 0usize;
        while cursor < conc.len() {
            let mut initium_c = cursor;
            cursor += 1;
            while cursor < conc.len()
                && (conc[cursor] - conc[cursor - 1] - 1) <= LIMEN_LACUNAE as usize
            {
                cursor += 1;
            }

            /* Reseca concordantias solitarias ab extremitatibus */
            while cursor > initium_c + 1 && (conc[cursor - 1] - conc[cursor - 2] - 1) > 0 {
                cursor -= 1;
            }
            while initium_c + 1 < cursor && (conc[initium_c + 1] - conc[initium_c] - 1) > 0 {
                initium_c += 1;
            }

            let num_in_trunco = cursor - initium_c;
            if num_in_trunco < LIMEN_TRUNCI as usize {
                continue;
            }

            let k_init = conc[initium_c];
            let k_fin = conc[cursor - 1];
            let magn = k_fin - k_init + 1;

            let t = Translatio {
                index_fontis: f_s,
                index_destinationis: f_d,
                initium_fontis: k_init,
                initium_dest: (k_init as i64 + dist) as usize,
                magnitudo: magn,
                concordantes: num_in_trunco,
                numerus_fontis: plicae[f_s].sublata.res[k_init].numerus,
                numerus_dest: plicae[f_d].addita.res[(k_init as i64 + dist) as usize].numerus,
            };
            candidati.push(t);
        }
    }
}

/* Electio avara */
fn elige_translationes(
    plicae: &mut Vec<Plica>,
    candidati: &mut Vec<Translatio>,
    translationes: &mut Vec<Translatio>,
) {
    /* Ordina secundum meritum netum (concordantes - dissimilia) decrescens */
    candidati.sort_by(|a, b| {
        let sa = 2 * a.concordantes as i64 - a.magnitudo as i64;
        let sb = 2 * b.concordantes as i64 - b.magnitudo as i64;
        if sa != sb {
            return sb.cmp(&sa);
        }
        b.concordantes.cmp(&a.concordantes)
    });

    for i in 0..candidati.len() {
        let c = candidati[i].clone();

        /* Inveni partes liberas (non possessas) intra candidatum */
        let mut initium_partis: Option<usize> = None;
        for k in 0..=c.magnitudo {
            let occupatum = if k == c.magnitudo {
                true
            } else {
                plicae[c.index_fontis].sublata.res[c.initium_fontis + k].translatum
                    || plicae[c.index_destinationis].addita.res[c.initium_dest + k].translatum
            };

            if !occupatum {
                if initium_partis.is_none() {
                    initium_partis = Some(k);
                }
            } else if let Some(ip) = initium_partis {
                let lon_partis = k - ip;
                if lon_partis >= LIMEN_TRUNCI as usize {
                    /* Aedifica translationem ex parte libera */
                    let mut pars = c.clone();
                    pars.initium_fontis += ip;
                    pars.initium_dest += ip;
                    pars.magnitudo = lon_partis;
                    pars.numerus_fontis =
                        plicae[c.index_fontis].sublata.res[pars.initium_fontis].numerus;
                    pars.numerus_dest =
                        plicae[c.index_destinationis].addita.res[pars.initium_dest].numerus;
                    pars.concordantes = 0;
                    for j in 0..lon_partis {
                        if plicae[c.index_fontis].sublata.res[pars.initium_fontis + j].sigillum
                            == plicae[c.index_destinationis].addita.res[pars.initium_dest + j]
                                .sigillum
                        {
                            pars.concordantes += 1;
                        }
                    }

                    /* Posside versus */
                    for j in 0..lon_partis {
                        plicae[c.index_fontis].sublata.res[pars.initium_fontis + j].translatum =
                            true;
                        plicae[c.index_destinationis].addita.res[pars.initium_dest + j]
                            .translatum = true;
                    }
                    translationes.push(pars);
                }
                initium_partis = None;
            }
        }
    }
}

/* Invenit omnes translationes inter omnia paria plicarum */
fn inveni_omnes(plicae: &mut Vec<Plica>, translationes: &mut Vec<Translatio>) {
    let mut candidati: Vec<Translatio> = Vec::new();
    let num_plic = plicae.len();
    for s in 0..num_plic {
        for d in 0..num_plic {
            if s != d {
                inveni_inter(plicae, &mut candidati, s, d);
            }
        }
    }
    elige_translationes(plicae, &mut candidati, translationes);
}

/* =========================================================================
 *  Scriptio formae ddiff ad stdout
 * ========================================================================= */

/* Iter canonicum plicae (non "/dev/null") */
fn iter_plicae(f: &Plica) -> &str {
    if f.genus == DELETUM {
        &f.iter_vetus
    } else {
        &f.iter_novus
    }
}

/* Littera generis */
fn littera_generis(genus: i32) -> char {
    match genus {
        DELETUM => 'D',
        CREATUM => 'C',
        _ => 'M',
    }
}

fn scribe_ddiff(plicae: &Vec<Plica>, translationes: &mut Vec<Translatio>) {
    let stdout = io::stdout();
    let mut scriptor = stdout.lock();

    /* ---- Caput ---- */
    writeln!(scriptor, "ddiff versio I").unwrap();
    writeln!(scriptor).unwrap();

    /* ---- PLICAE ---- */
    writeln!(scriptor, "PLICAE").unwrap();
    for f in plicae.iter() {
        writeln!(
            scriptor,
            "{} {}",
            littera_generis(f.genus),
            iter_plicae(f)
        )
        .unwrap();
    }
    writeln!(scriptor).unwrap();

    /* ---- TRANSLATIONES ---- */
    translationes.sort_by(|a, b| {
        if a.index_fontis != b.index_fontis {
            return a.index_fontis.cmp(&b.index_fontis);
        }
        a.initium_fontis.cmp(&b.initium_fontis)
    });

    writeln!(scriptor, "TRANSLATIONES {}", translationes.len()).unwrap();

    for t in translationes.iter() {
        let fs = &plicae[t.index_fontis];
        let fd = &plicae[t.index_destinationis];
        let it_f = &fs.iter_vetus;
        let it_d = &fd.iter_novus;
        let dissim = t.magnitudo - t.concordantes;
        let finis_fontis = fs.sublata.res[t.initium_fontis + t.magnitudo - 1].numerus;
        let finis_dest = fd.addita.res[t.initium_dest + t.magnitudo - 1].numerus;

        writeln!(
            scriptor,
            "T {}:[{},{}] >> {}:[{},{}] {}v {}d",
            it_f,
            t.numerus_fontis,
            finis_fontis,
            it_d,
            t.numerus_dest,
            finis_dest,
            t.magnitudo,
            dissim
        )
        .unwrap();

        /* Scribe delta dissimilium versuum */
        if dissim > 0 {
            let mut k = 0usize;
            while k < t.magnitudo {
                /* Salta versus concordantes */
                while k < t.magnitudo
                    && fs.sublata.res[t.initium_fontis + k].sigillum
                        == fd.addita.res[t.initium_dest + k].sigillum
                {
                    k += 1;
                }
                if k >= t.magnitudo {
                    break;
                }

                /* Collige dissimilia continua */
                let g_init = k;
                while k < t.magnitudo
                    && fs.sublata.res[t.initium_fontis + k].sigillum
                        != fd.addita.res[t.initium_dest + k].sigillum
                {
                    k += 1;
                }

                writeln!(scriptor, "  @{}", g_init).unwrap();
                for g in g_init..k {
                    writeln!(
                        scriptor,
                        "  -{}",
                        fs.sublata.res[t.initium_fontis + g].argumentum
                    )
                    .unwrap();
                }
                for g in g_init..k {
                    writeln!(
                        scriptor,
                        "  +{}",
                        fd.addita.res[t.initium_dest + g].argumentum
                    )
                    .unwrap();
                }
            }
        }
        writeln!(scriptor, ".").unwrap();
    }
    writeln!(scriptor).unwrap();

    /* ---- MUTATIONES ---- */
    writeln!(scriptor, "MUTATIONES").unwrap();
    for f in plicae.iter() {
        let mut habet = false;

        for v in f.sublata.res.iter() {
            if !v.translatum {
                if !habet {
                    writeln!(
                        scriptor,
                        "{} {}",
                        littera_generis(f.genus),
                        iter_plicae(f)
                    )
                    .unwrap();
                    habet = true;
                }
                writeln!(scriptor, "  -{} {}", v.numerus, v.argumentum).unwrap();
            }
        }
        for v in f.addita.res.iter() {
            if !v.translatum {
                if !habet {
                    writeln!(
                        scriptor,
                        "{} {}",
                        littera_generis(f.genus),
                        iter_plicae(f)
                    )
                    .unwrap();
                    habet = true;
                }
                writeln!(scriptor, "  +{} {}", v.numerus, v.argumentum).unwrap();
            }
        }
        if habet {
            writeln!(scriptor, ".").unwrap();
        }
    }

    /* ---- SUMMARIUM (ad stderr) ---- */
    let mut tot_trans = 0usize;
    let mut tot_dissim = 0usize;
    for t in translationes.iter() {
        tot_trans += t.magnitudo;
        tot_dissim += t.magnitudo - t.concordantes;
    }
    let mut rel_sub = 0usize;
    let mut rel_add = 0usize;
    for f in plicae.iter() {
        for v in f.sublata.res.iter() {
            if !v.translatum {
                rel_sub += 1;
            }
        }
        for v in f.addita.res.iter() {
            if !v.translatum {
                rel_add += 1;
            }
        }
    }
    eprintln!(
        "SUMMARIUM: {} plicae, {} translationes ({} versus, {} dissimilia)",
        plicae.len(),
        translationes.len(),
        tot_trans,
        tot_dissim
    );
    eprintln!("           reliqua: -{} +{} versus", rel_sub, rel_add);
}

/* =========================================================================
 *  Princeps
 * ========================================================================= */

fn main() {
    let mut plicae: Vec<Plica> = Vec::new();
    let mut translationes: Vec<Translatio> = Vec::new();

    resolve_differentiam(&mut plicae);
    inveni_omnes(&mut plicae, &mut translationes);
    scribe_ddiff(&plicae, &mut translationes);
}
