# Genera nucleos/ganglio/Temperamentos.hpp desde nucleos/ganglio/temperamentos.json
# Los 8 temperamentos son los del firmware Ganglio del Cardputer (extraídos de ganglio.bin, idénticos bit a bit).
# Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado · Escuela de Artes y Humanidades, Universidad EAFIT
import json, struct, os
d = json.load(open('nucleos/ganglio/temperamentos.json'))
def f(v): return repr(struct.unpack('<f', struct.pack('<f', v))[0]) + 'f'
nombres = {'LIBELULA': 'LIBÉLULA', 'LUCIERNAGA': 'LUCIÉRNAGA'}
descs = {'rapido e irregular': 'rápido e irregular', 'el mas sensible, casi caotico': 'el más sensible, casi caótico'}
L = ['// Temperamentos.hpp — GENERADO por tools/gen_temperamentos.py desde temperamentos.json. No editar a mano.',
     '// Redes de 8 neuronas center-crossing: 4 pares osciladores acoplados. W[i][j] = peso de j hacia i.',
     '#pragma once', '', 'namespace ganglio {', '',
     'struct Temperamento { const char* nombre; const char* caracter; float W[8][8]; float tau[8]; float theta[8]; };',
     'static const int N_TEMPERAMENTOS = %d;' % len(d),
     'static const Temperamento TEMPERAMENTOS[N_TEMPERAMENTOS] = {']
for t in d:
    W = ', '.join('{' + ', '.join(f(v) for v in r) + '}' for r in t['W'])
    L.append('  {"%s", "%s",\n   {%s},\n   {%s},\n   {%s}},' % (nombres.get(t['name'], t['name']), descs.get(t['desc'], t['desc']), W,
             ', '.join(f(v) for v in t['tau']), ', '.join(f(v) for v in t['theta'])))
L += ['};', '', '}  // namespace ganglio', '']
open('nucleos/ganglio/Temperamentos.hpp', 'w').write('\n'.join(L))
print('ok', len(d))
