// Scala.hpp — lectura de archivos de afinación Scala (.scl), el formato estándar de la microtonalidad
// https://www.huygens-fokker.org/scala/scl_format.html
// Cada grado es cents (lleva punto: 701.955) o razón (3/2, o un entero como 2). El último grado es el periodo.
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <cstdlib>

namespace espectros {

struct ScalaArchivo {
  std::string descripcion;
  std::vector<float> cents;   // grados dentro del periodo, empezando en 0
  float periodo = 1200.f;
  std::string error;
};

inline bool scalaParse(const std::string& texto, ScalaArchivo& out) {
  out = ScalaArchivo();
  std::istringstream in(texto);
  std::string linea;
  std::vector<std::string> utiles;
  while (std::getline(in, linea)) {
    if (!linea.empty() && linea.back() == '\r') linea.pop_back();
    if (!linea.empty() && linea[0] == '!') continue;
    utiles.push_back(linea);
  }
  if (utiles.size() < 2) { out.error = "archivo incompleto"; return false; }
  out.descripcion = utiles[0];
  const int n = atoi(utiles[1].c_str());
  if (n < 1 || n > 127) { out.error = "número de grados fuera de rango"; return false; }
  std::vector<float> grados;
  for (size_t i = 2; i < utiles.size() && (int)grados.size() < n; ++i) {
    std::istringstream ls(utiles[i]);
    std::string tok;
    if (!(ls >> tok)) continue;   // líneas vacías
    float c;
    if (tok.find('.') != std::string::npos) c = (float)atof(tok.c_str());
    else {
      const size_t sl = tok.find('/');
      const double a = atof(tok.substr(0, sl).c_str());
      const double b = sl == std::string::npos ? 1.0 : atof(tok.substr(sl + 1).c_str());
      if (!(a > 0) || !(b > 0)) { out.error = "razón inválida: " + tok; return false; }
      c = (float)(1200.0 * std::log2(a / b));
    }
    grados.push_back(c);
  }
  if ((int)grados.size() != n) { out.error = "faltan grados"; return false; }
  out.periodo = grados.back();
  if (!(out.periodo > 1.f)) { out.error = "periodo inválido"; return false; }
  out.cents.push_back(0.f);
  for (int i = 0; i < n - 1; ++i) out.cents.push_back(grados[i]);
  return true;
}

}  // namespace espectros
