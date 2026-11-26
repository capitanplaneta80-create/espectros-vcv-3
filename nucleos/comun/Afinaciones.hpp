// Afinaciones.hpp — catálogo de afinaciones no temperadas y no occidentales (generado desde ESPECTROS/tunings.js)
#pragma once

namespace espectros {
struct Afinacion { const char* nombre; const char* grupo; const char* nota; float periodo; int n; float cents[24]; };
static const int N_AFINACIONES = 38;
static const Afinacion AFINACIONES[N_AFINACIONES] = {
  {"Nativa (la física decide)", "Propia", "Sin atracción: cada motor suena con las alturas que da su fórmula (Bessel, Rydberg, Kepler…).", 1200.0000f, 0, {}},
  {"Slendro", "Indonesia", "Gamelan javanés: cinco pasos casi iguales, ninguno coincide con el piano.", 1200.0000f, 5, {0.000f, 231.000f, 474.000f, 717.000f, 955.000f}},
  {"Pelog (7 tonos)", "Indonesia", "Gamelan javanés: pasos pequeños y grandes alternados; cada conjunto tiene su propia versión.", 1200.0000f, 7, {0.000f, 120.000f, 270.000f, 540.000f, 670.000f, 785.000f, 950.000f}},
  {"Pelog bem (pathet)", "Indonesia", "Subconjunto de cinco tonos del pelog, como en el pathet lima.", 1200.0000f, 5, {0.000f, 120.000f, 270.000f, 670.000f, 785.000f}},
  {"Selisir (Bali)", "Indonesia", "Gong kebyar balinés; los instrumentos van en pares desafinados que baten (ombak).", 1200.0000f, 5, {0.000f, 125.000f, 290.000f, 690.000f, 805.000f}},
  {"22 shrutis", "India", "Los 22 microintervalos de la teoría clásica (entonación justa).", 1200.0000f, 22, {0.000f, 90.225f, 111.731f, 182.404f, 203.910f, 294.135f, 315.641f, 386.314f, 407.820f, 498.045f, 519.551f, 590.224f, 611.730f, 701.955f, 792.180f, 813.686f, 884.359f, 905.865f, 996.090f, 1017.596f, 1088.269f, 1109.775f}},
  {"Raga Bhairav", "India", "Raga del amanecer: re y dha bajos, en razones justas.", 1200.0000f, 7, {0.000f, 111.731f, 386.314f, 498.045f, 701.955f, 813.686f, 1088.269f}},
  {"Raga Yaman", "India", "Raga del anochecer, con ma tivra (cuarta aumentada).", 1200.0000f, 7, {0.000f, 203.910f, 386.314f, 590.224f, 701.955f, 905.865f, 1088.269f}},
  {"Raga Todi", "India", "Re, ga y dha muy bajos; intervalos pitagóricos.", 1200.0000f, 7, {0.000f, 90.225f, 294.135f, 590.224f, 701.955f, 792.180f, 1109.775f}},
  {"Raga Marwa", "India", "Hexatónico, sin quinta: tensión sin reposo.", 1200.0000f, 6, {0.000f, 111.731f, 386.314f, 590.224f, 884.359f, 1088.269f}},
  {"Maqam Rast", "Mundo árabe", "Tercera y séptima neutras: ni mayores ni menores.", 1200.0000f, 7, {0.000f, 204.000f, 355.000f, 498.000f, 702.000f, 906.000f, 1060.000f}},
  {"Maqam Bayati", "Mundo árabe", "Segunda de tres cuartos de tono: el sabor central del maqam.", 1200.0000f, 7, {0.000f, 145.000f, 294.000f, 498.000f, 702.000f, 792.000f, 996.000f}},
  {"Maqam Saba", "Mundo árabe", "Cuarta disminuida: el maqam del lamento.", 1200.0000f, 7, {0.000f, 150.000f, 300.000f, 400.000f, 702.000f, 800.000f, 1000.000f}},
  {"Maqam Hijaz", "Mundo árabe", "Segunda aumentada entre el segundo y el tercer grado.", 1200.0000f, 7, {0.000f, 114.000f, 384.000f, 498.000f, 702.000f, 792.000f, 996.000f}},
  {"Maqam Sikah", "Mundo árabe", "Empieza en una nota neutra: toda la escala queda “entre teclas”.", 1200.0000f, 7, {0.000f, 150.000f, 350.000f, 550.000f, 700.000f, 850.000f, 1050.000f}},
  {"Safi al-Din al-Urmawi (17)", "Mundo árabe", "Sistema de 17 tonos del siglo XIII, construido con quintas puras.", 1200.0000f, 17, {0.000f, 90.200f, 180.400f, 203.900f, 294.100f, 384.400f, 407.800f, 498.000f, 588.300f, 678.500f, 702.000f, 792.200f, 882.400f, 905.900f, 996.100f, 1086.300f, 1176.500f}},
  {"Dastgah Shur", "Persia", "Segundo grado koron (bajado unos 65 cents), según mediciones de Farhat.", 1200.0000f, 7, {0.000f, 135.000f, 294.000f, 498.000f, 702.000f, 792.000f, 996.000f}},
  {"Dastgah Chahargah", "Persia", "Dos segundas koron y terceras amplias.", 1200.0000f, 7, {0.000f, 135.000f, 386.000f, 498.000f, 702.000f, 835.000f, 1088.000f}},
  {"Makam Rast (53 comas)", "Turquía", "Segah y eviç una coma por debajo de las notas pitagóricas.", 1200.0000f, 7, {0.000f, 203.774f, 384.906f, 498.113f, 701.887f, 905.660f, 1086.792f}},
  {"Makam Uşşak", "Turquía", "Segundo grado ocho comas arriba: entre semitono y tono.", 1200.0000f, 7, {0.000f, 181.132f, 294.340f, 498.113f, 701.887f, 792.453f, 996.226f}},
  {"Makam Hicaz", "Turquía", "Tetracordio hicaz (5 + 12 + 5 comas) sobre pentacordio rast.", 1200.0000f, 7, {0.000f, 113.208f, 384.906f, 498.113f, 701.887f, 883.019f, 996.226f}},
  {"Tezeta menor", "Etiopía", "Kiñit de la nostalgia; en la práctica las alturas se mueven con cada cantor.", 1200.0000f, 5, {0.000f, 200.000f, 300.000f, 700.000f, 800.000f}},
  {"Bati", "Etiopía", "Kiñit con grandes saltos de tercera.", 1200.0000f, 5, {0.000f, 400.000f, 500.000f, 700.000f, 1100.000f}},
  {"Ambassel", "Etiopía", "Semitonos sobre la tónica y la quinta.", 1200.0000f, 5, {0.000f, 100.000f, 500.000f, 700.000f, 800.000f}},
  {"Anchihoye", "Etiopía", "El kiñit más inestable: tritono y sexta.", 1200.0000f, 5, {0.000f, 100.000f, 500.000f, 600.000f, 900.000f}},
  {"Mbira nyamaropa (Shona)", "África", "Heptatónica de la mbira dzavadzimu; cada instrumento tiene su propia afinación.", 1200.0000f, 7, {0.000f, 165.000f, 345.000f, 505.000f, 690.000f, 870.000f, 1035.000f}},
  {"Amadinda (Buganda)", "África", "Xilófono de la corte de Buganda: cinco pasos iguales (240 cents).", 1200.0000f, 5, {0.000f, 240.000f, 480.000f, 720.000f, 960.000f}},
  {"Timbila (Chopi)", "África", "Xilófonos de Mozambique: siete pasos casi iguales (≈171 cents).", 1200.0000f, 7, {0.000f, 171.429f, 342.857f, 514.286f, 685.714f, 857.143f, 1028.571f}},
  {"Thai (pi phat)", "Asia", "Siete pasos iguales de la orquesta pi phat; se tocan modos pentatónicos.", 1200.0000f, 7, {0.000f, 171.429f, 342.857f, 514.286f, 685.714f, 857.143f, 1028.571f}},
  {"Modo gong (sanfen sunyi)", "Asia", "Pentatónica china generada por quintas puras (“sumar y restar un tercio”).", 1200.0000f, 5, {0.000f, 203.910f, 407.820f, 701.955f, 905.865f}},
  {"Doce lü", "Asia", "Los doce tubos sonoros chinos: cadena de quintas puras.", 1200.0000f, 12, {0.000f, 113.700f, 203.900f, 317.600f, 407.800f, 521.500f, 611.700f, 702.000f, 815.600f, 905.900f, 1019.600f, 1109.800f}},
  {"Hirajōshi (koto)", "Asia", "Afinación del koto japonés, en razones justas.", 1200.0000f, 5, {0.000f, 203.910f, 315.641f, 701.955f, 813.686f}},
  {"Ryūkyū (Okinawa)", "Asia", "Escala del sanshin: terceras y séptimas puras.", 1200.0000f, 5, {0.000f, 386.314f, 498.045f, 701.955f, 1088.269f}},
  {"Serie armónica 8–16", "Espectrales", "Los armónicos 8 a 16 de una misma fundamental: la escala de la física.", 1200.0000f, 8, {0.000f, 203.910f, 386.314f, 551.318f, 701.955f, 840.528f, 968.826f, 1088.269f}},
  {"Serie subarmónica 16–8", "Espectrales", "Undertones: el espejo de la serie armónica.", 1200.0000f, 8, {0.000f, 111.731f, 231.174f, 359.472f, 498.045f, 648.682f, 813.686f, 996.090f}},
  {"Bohlen-Pierce (tritava)", "Espectrales", "13 pasos justos que se repiten cada 3:1, no cada octava.", 1901.9550f, 13, {0.000f, 133.238f, 301.847f, 435.084f, 582.512f, 736.931f, 884.359f, 1017.596f, 1165.024f, 1319.443f, 1466.871f, 1600.108f, 1768.717f}},
  {"12-TET (referencia)", "Temperadas", "La octava en doce partes iguales: el piano, como punto de comparación.", 1200.0000f, 12, {0.000f, 100.000f, 200.000f, 300.000f, 400.000f, 500.000f, 600.000f, 700.000f, 800.000f, 900.000f, 1000.000f, 1100.000f}},
  {"19-EDO", "Temperadas", "Diecinueve pasos iguales: terceras casi puras.", 1200.0000f, 19, {0.000f, 63.158f, 126.316f, 189.474f, 252.632f, 315.789f, 378.947f, 442.105f, 505.263f, 568.421f, 631.579f, 694.737f, 757.895f, 821.053f, 884.211f, 947.368f, 1010.526f, 1073.684f, 1136.842f}},
};
}  // namespace espectros
