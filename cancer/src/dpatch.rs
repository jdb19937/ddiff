/* =========================================================================
 *  DPATCH — INSTRUMENTUM APPLICATIONIS DIFFERENTIARUM COMPACTARUM
 *
 *  Hoc instrumentum formam compactam "ddiff" legit e flumine stdin
 *  et mutationes in fasciculos locales applicat.
 *
 *  Translatio fidelis ex lingua C in linguam Rust.
 *
 *  Compilatio:  cargo build --release --bin dpatch
 *  Usus:        ./dpatch < exemplum.ddiff
 *
 *  Anno MMXXVI
 * ========================================================================= */

use std::collections::HashMap;
use std::fs;
use std::io::{self, Read, Write};
use std::path::Path;

/* ---------- Genera fasciculorum ---------- */

const MUTATUM: i32 = 0;
const DELETUM: i32 = 1;
const CREATUM: i32 = 2;

/* ---------- Typi principales ---------- */

struct Fasciculus {
    iter: String, /* iter fasciculi                      */
    genus: i32,   /* DELETUM / CREATUM / MUTATUM         */
}

struct Translatio {
    iter_fontis: String,                 /* iter fasciculi fontis               */
    initium_fontis: i32,                 /* numerus versus initialis in fonte   */
    finis_fontis: i32,                   /* numerus versus finalis in fonte     */
    iter_dest: String,                   /* iter fasciculi destinationis        */
    initium_dest: i32,                   /* numerus versus initialis in dest    */
    finis_dest: i32,                     /* numerus versus finalis in dest      */
    magnitudo: i32,                      /* versus totales (Nv)                 */
    #[allow(dead_code)]
    dissimilia: i32,                     /* versus dissimiles (Md)              */
    substitutiones: Vec<Option<String>>, /* [0..magnitudo-1], None = identicum */
}

struct VersusOperatio {
    numerus: i32,       /* numerus versus in fasciculo         */
    additum: bool,      /* verum = additum (+), falsum = sublatum (-) */
    argumentum: String, /* textus versus                       */
}

struct MutatioFasciculi {
    iter: String, /* iter fasciculi                      */
    #[allow(dead_code)]
    genus: i32,   /* genus fasciculi                     */
    operationes: Vec<VersusOperatio>,
}

struct VersusAdditus {
    numerus: i32,       /* numerus versus destinationis         */
    argumentum: String, /* textus versus                        */
}

/* =========================================================================
 *  Lectio fasciculi e disco (cum cache)
 * ========================================================================= */

fn lege_fasciculum(iter: &str) -> Vec<String> {
    let argumentum = fs::read_to_string(iter).unwrap_or_else(|err| {
        eprintln!("ERROR: fasciculus '{}' aperiri non potest: {}", iter, err);
        std::process::exit(1);
    });
    /* versus[0] vacuus est (index 1-based) */
    let mut versus: Vec<String> = vec![String::new()];
    for linea in argumentum.lines() {
        versus.push(linea.to_string());
    }
    versus
}

fn cape_fasciculum<'a>(cache: &'a mut HashMap<String, Vec<String>>, iter: &str) -> &'a Vec<String> {
    if !cache.contains_key(iter) {
        let versus = lege_fasciculum(iter);
        cache.insert(iter.to_string(), versus);
    }
    cache.get(iter).unwrap()
}

/* =========================================================================
 *  Resolutio formae ddiff ex stdin
 * ========================================================================= */

fn genus_ex_littera(c: char) -> i32 {
    match c {
        'D' => DELETUM,
        'C' => CREATUM,
        _ => MUTATUM,
    }
}

fn lege_numerum(octeti: &[u8], initium: usize) -> (i32, usize) {
    let mut pos = initium;
    let mut valor: i32 = 0;
    while pos < octeti.len() && octeti[pos] >= b'0' && octeti[pos] <= b'9' {
        valor = valor * 10 + (octeti[pos] - b'0') as i32;
        pos += 1;
    }
    (valor, pos)
}

struct StaturDdiff {
    fasciculi: Vec<Fasciculus>,
    translationes: Vec<Translatio>,
    mutat_fasc: Vec<MutatioFasciculi>,
}

fn resolve_ddiff() -> StaturDdiff {
    /* Lege totum flumen stdin */
    let mut thesaurus = String::new();
    io::stdin()
        .read_to_string(&mut thesaurus)
        .expect("ERROR: stdin legi non potest");

    let versus: Vec<&str> = thesaurus.lines().collect();
    let num_vs = versus.len();
    let mut i = 0usize;

    let mut fasciculi: Vec<Fasciculus> = Vec::new();
    let mut translationes: Vec<Translatio> = Vec::new();
    let mut mutat_fasc: Vec<MutatioFasciculi> = Vec::new();

    /* ---- I. Caput: "ddiff versio I" ---- */
    if i >= num_vs || versus[i] != "ddiff versio I" {
        eprintln!("ERROR: caput 'ddiff versio I' deest");
        std::process::exit(1);
    }
    i += 1;
    while i < num_vs && versus[i].is_empty() {
        i += 1;
    }

    /* ---- II. FASCICULI ---- */
    if i >= num_vs || versus[i] != "FASCICULI" {
        eprintln!("ERROR: sectio FASCICULI deest");
        std::process::exit(1);
    }
    i += 1;

    while i < num_vs && !versus[i].is_empty() {
        let lin = versus[i];
        let octeti = lin.as_bytes();
        if octeti.len() >= 3
            && (octeti[0] == b'D' || octeti[0] == b'C' || octeti[0] == b'M')
            && octeti[1] == b' '
        {
            fasciculi.push(Fasciculus {
                genus: genus_ex_littera(octeti[0] as char),
                iter: lin[2..].to_string(),
            });
        }
        i += 1;
    }
    while i < num_vs && versus[i].is_empty() {
        i += 1;
    }

    /* ---- III. TRANSLATIONES ---- */
    if i >= num_vs || !versus[i].starts_with("TRANSLATIONES") {
        eprintln!("ERROR: sectio TRANSLATIONES deest");
        std::process::exit(1);
    }
    i += 1;

    while i < num_vs {
        let lin = versus[i];
        if lin.is_empty() {
            break;
        }
        if lin.starts_with("MUTATIONES") {
            break;
        }

        if !lin.starts_with("T ") {
            i += 1;
            continue;
        }

        /* Resolve caput translationis:
         *   T iter_fontis:[a,b] >> iter_dest:[c,d] Nv Md */
        let p = &lin[2..];
        let col1 = match p.find(":[") {
            Some(pos) => pos,
            None => {
                i += 1;
                continue;
            }
        };

        let iter_fontis = p[..col1].to_string();
        let octeti = p.as_bytes();
        let mut pos = col1 + 2;

        let (initium_fontis, p2) = lege_numerum(octeti, pos);
        pos = p2;
        if pos < octeti.len() && octeti[pos] == b',' {
            pos += 1;
        }
        let (finis_fontis, p3) = lege_numerum(octeti, pos);
        pos = p3;

        /* Quaere " >> " */
        let reliquum = &p[pos..];
        let sep = match reliquum.find(" >> ") {
            Some(s) => s,
            None => {
                i += 1;
                continue;
            }
        };
        let post_sep = &reliquum[sep + 4..];
        let col2 = match post_sep.find(":[") {
            Some(s) => s,
            None => {
                i += 1;
                continue;
            }
        };

        let iter_dest = post_sep[..col2].to_string();
        let octeti_d = post_sep.as_bytes();
        let mut pos_d = col2 + 2;

        let (initium_dest, p4) = lege_numerum(octeti_d, pos_d);
        pos_d = p4;
        if pos_d < octeti_d.len() && octeti_d[pos_d] == b',' {
            pos_d += 1;
        }
        let (finis_dest, p5) = lege_numerum(octeti_d, pos_d);
        pos_d = p5;

        /* Transili ad spatium ante Nv */
        while pos_d < octeti_d.len() && octeti_d[pos_d] != b' ' {
            pos_d += 1;
        }
        while pos_d < octeti_d.len() && octeti_d[pos_d] == b' ' {
            pos_d += 1;
        }
        let (magnitudo, p6) = lege_numerum(octeti_d, pos_d);
        pos_d = p6;
        if pos_d < octeti_d.len() && octeti_d[pos_d] == b'v' {
            pos_d += 1;
        }
        while pos_d < octeti_d.len() && octeti_d[pos_d] == b' ' {
            pos_d += 1;
        }
        let (dissimilia, _) = lege_numerum(octeti_d, pos_d);

        /* Crea seriem substitutionum */
        let mut substitutiones: Vec<Option<String>> = vec![None; magnitudo as usize];

        /* Lege versus delta usque ad punctum terminale */
        i += 1;
        let mut dist_cur: i32 = -1;
        let mut plus_idx: i32 = 0;
        while i < num_vs && versus[i] != "." {
            let lin_d = versus[i];
            if lin_d.starts_with("  @") {
                dist_cur = lin_d[3..].parse::<i32>().unwrap_or(0);
                plus_idx = 0;
            } else if lin_d.starts_with("  -") {
                /* Versus fontis — non opus est, e fasciculo legemus */
            } else if lin_d.starts_with("  +") {
                if dist_cur >= 0 && (dist_cur + plus_idx) < magnitudo {
                    substitutiones[(dist_cur + plus_idx) as usize] = Some(lin_d[3..].to_string());
                }
                plus_idx += 1;
            }
            i += 1;
        }
        if i < num_vs {
            i += 1; /* transili punctum "." */
        }

        translationes.push(Translatio {
            iter_fontis,
            initium_fontis,
            finis_fontis,
            iter_dest,
            initium_dest,
            finis_dest,
            magnitudo,
            dissimilia,
            substitutiones,
        });
        continue;
    }
    while i < num_vs && versus[i].is_empty() {
        i += 1;
    }

    /* ---- IV. MUTATIONES ---- */
    if i >= num_vs || !versus[i].starts_with("MUTATIONES") {
        return StaturDdiff {
            fasciculi,
            translationes,
            mutat_fasc,
        };
    }
    i += 1;

    while i < num_vs {
        let lin = versus[i];
        if lin.is_empty() {
            i += 1;
            continue;
        }

        let octeti = lin.as_bytes();
        if octeti.len() < 3 || !matches!(octeti[0], b'D' | b'C' | b'M') || octeti[1] != b' ' {
            i += 1;
            continue;
        }

        let mut mf = MutatioFasciculi {
            genus: genus_ex_littera(octeti[0] as char),
            iter: lin[2..].to_string(),
            operationes: Vec::new(),
        };

        i += 1;
        while i < num_vs && versus[i] != "." {
            let lin_m = versus[i];
            if lin_m.starts_with("  -") || lin_m.starts_with("  +") {
                let additum = lin_m.as_bytes()[2] == b'+';
                let p = &lin_m[3..];
                let octeti_p = p.as_bytes();
                let (numerus, pos) = lege_numerum(octeti_p, 0);
                let argumentum = if pos < octeti_p.len() && octeti_p[pos] == b' ' {
                    p[pos + 1..].to_string()
                } else {
                    p[pos..].to_string()
                };
                mf.operationes.push(VersusOperatio {
                    numerus,
                    additum,
                    argumentum,
                });
            }
            i += 1;
        }
        if i < num_vs {
            i += 1; /* transili punctum "." */
        }

        mutat_fasc.push(mf);
    }

    StaturDdiff {
        fasciculi,
        translationes,
        mutat_fasc,
    }
}

/* =========================================================================
 *  Applicatio: aedificatio fasciculorum novorum
 * ========================================================================= */

/* Resolve argumentum destinationis pro indice k intra translationem */
fn resolve_versum(t: &Translatio, k: i32, fons: &Vec<String>) -> String {
    if let Some(ref sub) = t.substitutiones[k as usize] {
        return sub.clone();
    }
    let lin = t.initium_fontis + k;
    if lin >= 1 && (lin as usize) < fons.len() {
        return fons[lin as usize].clone();
    }
    String::new()
}

/* Fasciculus creatus: omnes versus ex translationibus et mutationibus */
fn aedifica_creatum(
    iter: &str,
    translationes: &[Translatio],
    mutat_fasc: &[MutatioFasciculi],
    cache: &mut HashMap<String, Vec<String>>,
) -> Option<Vec<String>> {
    let mut maximus: i32 = 0;

    for t in translationes.iter() {
        if t.iter_dest == iter && t.finis_dest > maximus {
            maximus = t.finis_dest;
        }
    }

    for mf in mutat_fasc.iter() {
        if mf.iter == iter {
            for op in mf.operationes.iter() {
                if op.additum && op.numerus > maximus {
                    maximus = op.numerus;
                }
            }
        }
    }

    if maximus == 0 {
        return None;
    }

    /* versus[0] vacuus, versus[1..maximus] */
    let mut versus: Vec<Option<String>> = vec![None; (maximus + 1) as usize];

    /* Imple ex translationibus */
    for t in translationes.iter() {
        if t.iter_dest != iter {
            continue;
        }
        let fons = cape_fasciculum(cache, &t.iter_fontis).clone();

        for k in 0..t.magnitudo {
            let lin_dest = t.initium_dest + k;
            if lin_dest >= 1 && lin_dest <= maximus {
                versus[lin_dest as usize] = Some(resolve_versum(t, k, &fons));
            }
        }
    }

    /* Imple ex mutationibus */
    for mf in mutat_fasc.iter() {
        if mf.iter != iter {
            continue;
        }
        for op in mf.operationes.iter() {
            if op.additum && op.numerus >= 1 && op.numerus <= maximus {
                versus[op.numerus as usize] = Some(op.argumentum.clone());
            }
        }
    }

    /* Converte in Vec<String> (1-indexed) */
    let mut resultatum: Vec<String> = vec![String::new()]; /* index 0 */
    for idx in 1..=(maximus as usize) {
        resultatum.push(versus[idx].clone().unwrap_or_default());
    }
    Some(resultatum)
}

/* Fasciculus mutatus: veteres versus emendantur */
fn aedifica_mutatum(
    iter: &str,
    translationes: &[Translatio],
    mutat_fasc: &[MutatioFasciculi],
    cache: &mut HashMap<String, Vec<String>>,
) -> Option<Vec<String>> {
    /* I. Lege fasciculum veterem */
    let veteres = cape_fasciculum(cache, iter).clone();
    let num_vet = veteres.len() - 1; /* index 0 vacuus */

    /* II. Nota versus sublatos */
    let mut sublatum: Vec<bool> = vec![false; num_vet + 1];

    for t in translationes.iter() {
        if t.iter_fontis != iter {
            continue;
        }
        for k in t.initium_fontis..=t.finis_fontis {
            if k >= 1 && (k as usize) <= num_vet {
                sublatum[k as usize] = true;
            }
        }
    }

    for mf in mutat_fasc.iter() {
        if mf.iter != iter {
            continue;
        }
        for op in mf.operationes.iter() {
            if !op.additum && op.numerus >= 1 && (op.numerus as usize) <= num_vet {
                sublatum[op.numerus as usize] = true;
            }
        }
    }

    /* III. Collige versus additos */
    let mut addita: Vec<VersusAdditus> = Vec::new();

    for t in translationes.iter() {
        if t.iter_dest != iter {
            continue;
        }
        let fons = cape_fasciculum(cache, &t.iter_fontis).clone();

        for k in 0..t.magnitudo {
            addita.push(VersusAdditus {
                numerus: t.initium_dest + k,
                argumentum: resolve_versum(t, k, &fons),
            });
        }
    }

    for mf in mutat_fasc.iter() {
        if mf.iter != iter {
            continue;
        }
        for op in mf.operationes.iter() {
            if !op.additum {
                continue;
            }
            addita.push(VersusAdditus {
                numerus: op.numerus,
                argumentum: op.argumentum.clone(),
            });
        }
    }

    addita.sort_by_key(|a| a.numerus);

    /* IV. Computa longitudinem novam */
    let num_sublata = sublatum[1..].iter().filter(|&&s| s).count();
    let num_nov = num_vet - num_sublata + addita.len();

    /* V. Aedifica fasciculum novum */
    let mut novi: Vec<String> = vec![String::new()]; /* index 0 vacuus */
    let mut cursor_vet: usize = 1;
    let mut cursor_nov: usize = 1;
    let mut idx_add: usize = 0;

    while cursor_nov <= num_nov {
        if idx_add < addita.len() && addita[idx_add].numerus == cursor_nov as i32 {
            novi.push(addita[idx_add].argumentum.clone());
            idx_add += 1;
        } else {
            while cursor_vet <= num_vet && sublatum[cursor_vet] {
                cursor_vet += 1;
            }
            if cursor_vet <= num_vet {
                novi.push(veteres[cursor_vet].clone());
                cursor_vet += 1;
            } else {
                novi.push(String::new());
            }
        }
        cursor_nov += 1;
    }

    Some(novi)
}

/* =========================================================================
 *  Scriptio et deletio fasciculorum
 * ========================================================================= */

/* Creat directoria parentalia (similis mkdir -p) */
fn crea_directoria(iter: &str) {
    if let Some(parens) = Path::new(iter).parent() {
        if !parens.as_os_str().is_empty() {
            let _ = fs::create_dir_all(parens);
        }
    }
}

fn scribe_fasciculum(iter: &str, versus: &[String]) {
    crea_directoria(iter);
    let mut f = fs::File::create(iter).unwrap_or_else(|err| {
        eprintln!("ERROR: fasciculus '{}' scribi non potest: {}", iter, err);
        std::process::exit(1);
    });
    /* versus[0] vacuus est; scribe versus[1..] */
    for idx in 1..versus.len() {
        writeln!(f, "{}", versus[idx]).unwrap();
    }
}

/* =========================================================================
 *  Applicatio principalis
 * ========================================================================= */

fn applica(status: StaturDdiff) {
    eprintln!(
        "APPLICATIO: {} fasciculi, {} translationes",
        status.fasciculi.len(),
        status.translationes.len()
    );

    let mut cache: HashMap<String, Vec<String>> = HashMap::new();

    /* Phase I: aedifica argumenta nova in memoria */
    let mut argumenta: Vec<Option<Vec<String>>> = Vec::new();

    for f in status.fasciculi.iter() {
        if f.genus == CREATUM {
            argumenta.push(aedifica_creatum(
                &f.iter,
                &status.translationes,
                &status.mutat_fasc,
                &mut cache,
            ));
        } else if f.genus == MUTATUM {
            argumenta.push(aedifica_mutatum(
                &f.iter,
                &status.translationes,
                &status.mutat_fasc,
                &mut cache,
            ));
        } else {
            argumenta.push(None);
        }
    }

    /* Phase II: scribe fasciculos creatos et mutatos */
    let mut num_cre = 0;
    let mut num_mut_n = 0;
    let mut num_del = 0;

    for (idx, f) in status.fasciculi.iter().enumerate() {
        if f.genus == CREATUM {
            if let Some(ref vs) = argumenta[idx] {
                scribe_fasciculum(&f.iter, vs);
                eprintln!("  C {} ({} versus)", f.iter, vs.len() - 1);
                num_cre += 1;
            }
        } else if f.genus == MUTATUM {
            if let Some(ref vs) = argumenta[idx] {
                scribe_fasciculum(&f.iter, vs);
                eprintln!("  M {} ({} versus)", f.iter, vs.len() - 1);
                num_mut_n += 1;
            }
        }
    }

    /* Phase III: dele fasciculos */
    for f in status.fasciculi.iter() {
        if f.genus != DELETUM {
            continue;
        }
        match fs::remove_file(&f.iter) {
            Ok(()) => {
                eprintln!("  D {}", f.iter);
                num_del += 1;
            }
            Err(err) if err.kind() != io::ErrorKind::NotFound => {
                eprintln!(
                    "MONITUM: fasciculus '{}' deleri non potest: {}",
                    f.iter, err
                );
            }
            _ => {}
        }
    }

    eprintln!(
        "FACTUM: {} creati, {} mutati, {} deleti",
        num_cre, num_mut_n, num_del
    );
}

/* =========================================================================
 *  Princeps
 * ========================================================================= */

fn main() {
    let status = resolve_ddiff();
    applica(status);
}
