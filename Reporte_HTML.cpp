#include "Reporte_HTML.hpp"

#include <chrono>
#include <fstream>
#include <initializer_list>
#include <stdexcept>

namespace {

void Escribir_Texto(std::ostream& Salida, std::string_view Texto) {

    char Digitos[] = "0123456789abcdef";
    Salida << '"';
    for (unsigned char Caracter : Texto) {
        if (Caracter == '"' || Caracter == '\\') {
            Salida << '\\' << Caracter;
        }

        else if (Caracter < 32 || Caracter == '<' || Caracter == '>' || Caracter == '&') {
            Salida << "\\u00" << Digitos[Caracter / 16] << Digitos[Caracter % 16];
        }

        else {
            Salida << Caracter;
        }
    }
    Salida << '"';
}

void Escribir_Fila(std::ostream& Salida, std::initializer_list<std::string_view> Celdas) {

    Salida << '[';
    std::string_view Separador;
    for (std::string_view Celda : Celdas) {
        Salida << Separador;
        Escribir_Texto(Salida, Celda);
        Separador = ",";
    }
    Salida << ']';
}

std::string_view Grupo_Nodo(Tipo_Nodo Clase) {

    switch (Clase) {
        case Tipo_Nodo::Program:
            return "programa";
        case Tipo_Nodo::ClassDef: case Tipo_Nodo::FunctionDef: case Tipo_Nodo::VarDef:
        case Tipo_Nodo::Parameter: case Tipo_Nodo::GlobalDecl: case Tipo_Nodo::NonlocalDecl:
            return "declaracion";
        case Tipo_Nodo::TypeName: case Tipo_Nodo::ListType:
            return "tipo";
        case Tipo_Nodo::Block: case Tipo_Nodo::If: case Tipo_Nodo::Elif: case Tipo_Nodo::Else:
        case Tipo_Nodo::While: case Tipo_Nodo::For: case Tipo_Nodo::Pass: case Tipo_Nodo::Return:
        case Tipo_Nodo::ExprStmt: case Tipo_Nodo::Assign:
            return "sentencia";
        default:
            return "expresion";
    }
}

std::string_view Rol_Nodo(AST& Arbol, Nodo_AST& Padre, size_t Indice) {

    switch (Padre.Clase) {
        case Tipo_Nodo::ClassDef: return Indice == 0 ? "Superclase" : "Miembro";
        case Tipo_Nodo::FunctionDef: {
            Tipo_Nodo Clase = Arbol[Padre.Hijos[Indice]].Clase;
            if (Clase == Tipo_Nodo::Parameter) return "Parámetro";
            if (Clase == Tipo_Nodo::TypeName || Clase == Tipo_Nodo::ListType) return "Tipo de retorno";
            return {};
        }
        case Tipo_Nodo::Parameter: return "Tipo declarado";
        case Tipo_Nodo::VarDef: return Indice == 0 ? "Tipo declarado" : "Valor inicial";
        case Tipo_Nodo::ListType: return "Tipo del elemento";
        case Tipo_Nodo::If: return Indice == 0 ? "Condición" : Indice == 1 ? "Entonces" : "Alternativa";
        case Tipo_Nodo::Elif: return Indice == 0 ? "Condición" : "Entonces";
        case Tipo_Nodo::While: return Indice == 0 ? "Condición" : "Cuerpo";
        case Tipo_Nodo::For: return Indice == 0 ? "Variable" : Indice == 1 ? "Iterable" : "Cuerpo";
        case Tipo_Nodo::Assign: return Indice + 1 == Padre.Hijos.size() ? "Valor" : "Destino";
        case Tipo_Nodo::Conditional: return Indice == 0 ? "Condición" : Indice == 1 ? "Si es verdadera" : "Si es falsa";
        case Tipo_Nodo::Binary: return Indice == 0 ? "Izquierdo" : "Derecho";
        case Tipo_Nodo::Index: return Indice == 0 ? "Colección" : "Índice";
        case Tipo_Nodo::Call: return Indice == 0 ? "Invocable" : "Argumento";
        case Tipo_Nodo::Member: return "Receptor";
        case Tipo_Nodo::Return: return "Valor de retorno";
        default: return {};
    }
}

std::vector<bool> Nodos_Visibles(AST& Arbol) {

    std::vector<bool> Visibles(Arbol.Cantidad_Nodos() + 1);
    Lista_Nodos Pendientes{Arbol.Raiz};
    while (!Pendientes.empty()) {
        Id_Nodo Id = Pendientes.back();
        Pendientes.pop_back();
        if (!Id || Visibles[Id]) continue;
        Visibles[Id] = true;
        const auto& Hijos = Arbol[Id].Hijos;
        Pendientes.insert(Pendientes.end(), Hijos.begin(), Hijos.end());
    }
    return Visibles;
}

void Escribir_AST(std::ostream& Salida, AST& Arbol, const std::vector<bool>& Visibles) {

    bool Parcial = Arbol.Raiz && Arbol[Arbol.Raiz].Incompleto;
    Salida << ",\"Raiz\":" << Arbol.Raiz << ",\"Parcial\":" << (Parcial ? "true" : "false") << ",\"Nodos\":[null";
    if (Arbol.Raiz) {
        for (Id_Nodo Id = 1; Id <= Arbol.Cantidad_Nodos(); Id++) {
            if (!Visibles[Id]) {
                Salida << ",null";
                continue;
            }
            Nodo_AST& Nodo = Arbol[Id];
            Salida << ",{\"Clase\":";
            Escribir_Texto(Salida, Nombre_Nodo(Nodo.Clase));
            Salida << ",\"Grupo\":";
            Escribir_Texto(Salida, Grupo_Nodo(Nodo.Clase));
            Salida << ",\"Texto\":";
            Escribir_Texto(Salida, Nodo.Texto);
            Salida << ",\"Tipo\":";
            Escribir_Texto(Salida, Nodo.Tipo);
            Salida << ",\"Linea\":" << Nodo.Posicion.Linea << ",\"Columna\":" << Nodo.Posicion.Columna
                   << ",\"Error\":" << (Nodo.Tiene_Error ? "true" : "false") << ",\"Hijos\":[";
            for (size_t I = 0; I < Nodo.Hijos.size(); I++) {
                if (I) Salida << ',';
                Salida << '[' << Nodo.Hijos[I] << ',';
                Escribir_Texto(Salida, Rol_Nodo(Arbol, Nodo, I));
                Salida << ']';
            }
            Salida << "]}\n";
        }
    }

    Salida << ']';
}

void Escribir_Seguimiento(std::ostream& Salida, Seguimiento& Traza) {

    Salida << ",\"Pasos\":[";
    for (size_t I = 0; I < Traza.Pasos.size(); I++) {
        if (I) Salida << ',';
        Paso_Seguimiento& Paso = Traza.Pasos[I];
        Escribir_Fila(Salida, {Paso.Fase, Paso.Estado, Paso.Simbolos, Paso.Entrada, Paso.Accion,
                             Paso.Atributos, Paso.Accion_AST, Paso.Nodo ? std::to_string(Paso.Nodo) : "",
                             Paso.Linea ? std::to_string(Paso.Linea) : ""});
    }
    Salida << ']';
}

void Escribir_Tablas(std::ostream& Salida, std::vector<Tabla_Ambito>& Tablas) {

    Salida << ",\"Ambitos\":[";
    for (size_t I = 0; I < Tablas.size(); I++) {
        if (I) Salida << ',';
        Tabla_Ambito& Tabla = Tablas[I];
        Salida << "{\"Nombre\":";
        Escribir_Texto(Salida, Tabla.Nombre);
        Salida << ",\"Padre\":";
        Escribir_Texto(Salida, Tabla.Padre);
        Salida << ",\"Superclase\":";
        Escribir_Texto(Salida, Tabla.Superclase);
        Salida << ",\"Simbolos\":[";

        for (size_t J = 0; J < Tabla.Simbolos.size(); J++) {
            if (J) Salida << ',';
            Fila_Symbol& Simbolo = Tabla.Simbolos[J];
            Escribir_Fila(Salida, {Simbolo.Nombre, Simbolo.Categoria, Simbolo.Tipo, Simbolo.Declaracion, Simbolo.Origen});
        }
        Salida << "]}";
    }

    Salida << ']';
}

void Escribir_Errores(std::ostream& Salida, Seguimiento& Traza, const std::vector<bool>& Visibles) {

    Salida << ",\"Errores\":[";
    for (size_t I = 0; I < Traza.Errores.size(); I++) {
        if (I) Salida << ',';
        Error_Compilacion& Error = Traza.Errores[I];
        Salida << "{\"Fase\":";
        Escribir_Texto(Salida, Error.Fase);
        Salida << ",\"Mensaje\":";
        Escribir_Texto(Salida, Error.Mensaje);
        Salida << ",\"Linea\":" << Error.Posicion.Linea << ",\"Columna\":" << Error.Posicion.Columna
               << ",\"Nodo\":" << (Visibles[Error.Nodo] ? Error.Nodo : 0) << '}';
    }

    Salida << ']';
}

constexpr char Inicio_HTML[] = R"HTML(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ChocoPy · Reporte de compilación</title>
<style>
  :root{font:15px/1.5 system-ui,sans-serif;color:#17243b;background:#f4f6fa;--borde:#dbe2ee;--azul:#253dd2;--suave:#4c5d75}
  *{box-sizing:border-box}
  body{margin:0}
  [hidden]{display:none!important}
  header{background:#14223b;color:#fff}
  .encabezado,main{max-width:1480px;margin:auto;padding:22px clamp(14px,4vw,54px)}
  .encabezado p{margin:0;color:#bfcef0;font:600 13px ui-monospace,monospace}
  h1{margin:4px 0 14px;font-size:clamp(26px,4vw,39px);line-height:1.2;letter-spacing:-.03em}
  h2,h3{margin:0;font-size:17px}
  .archivo{font:14px ui-monospace,monospace;overflow-wrap:anywhere}
  .estado{display:inline-block;margin-left:8px;border-radius:20px;padding:3px 9px;color:#134d37;background:#d9f6e5;font:600 12px system-ui}
  .estado.error{color:#822139;background:#ffe1e9}
  nav{display:flex;overflow-x:auto;gap:5px;margin-bottom:16px;border-bottom:1px solid var(--borde)}
  button{font:inherit;cursor:pointer;border:1px solid var(--borde);background:#fff;color:#243657;border-radius:7px;padding:6px 10px}
  button:hover{background:#edf1ff}
  button:disabled{color:#6a778a;cursor:default}
  [role=tab]{white-space:nowrap;border:0;border-radius:0;background:transparent;padding:10px 13px;border-bottom:3px solid transparent}
  [role=tab][aria-selected=true]{border-bottom-color:var(--azul);color:#233aaf;font-weight:700}
  :focus-visible{outline:2px solid var(--azul);outline-offset:2px}
  section,.arbol,.detalle{background:#fff;border:1px solid var(--borde);border-radius:12px;min-width:0}
  section{padding:15px}
  label{font-weight:620}
  input,select{font:inherit;border:1px solid #adb9d0;border-radius:7px;padding:7px 9px;background:#fff;color:inherit;min-width:0;max-width:100%}
  .barra{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:10px}
  .barra input{flex:1 1 190px}
  .meta,.rol{color:var(--suave);font-size:12px}
  .tabla{overflow:auto;max-height:65vh}
  table{border-collapse:separate;border-spacing:0;width:100%;text-align:left;font-size:13px}
  th{background:#edf1f8;position:sticky;top:0;white-space:nowrap;z-index:1}
  th,td{padding:8px 9px;border-bottom:1px solid var(--borde);vertical-align:top}
  td{min-width:68px;overflow-wrap:anywhere}
  tr:nth-child(even) td{background:#f8faff}
  .mono,code{font-family:ui-monospace,Consolas,monospace}
  #tabla-seguimiento td:nth-child(n+4){min-width:105px}
  #tabla-seguimiento td:nth-child(6),#tabla-seguimiento td:nth-child(7){min-width:138px}
  #tabla-seguimiento td:nth-child(8){min-width:260px}
  #tabla-seguimiento td.accion-con-nodo{background:#e9eeff;box-shadow:inset 3px 0 #5b72c9}
  .paso-accion{display:block}
  .accion-ast{display:inline-block;margin-top:3px}
  button.accion-ast,.enlace-ambito{font:inherit;background:transparent;border:0;padding:0;text-align:left;color:#243e99;text-decoration:underline;text-underline-offset:3px}
  button.accion-ast:hover,.enlace-ambito:hover{color:#182963;background:transparent}
  .arbol button:focus-visible{outline-color:#a9c7ff}
  .guia-traza{margin:0 0 10px;color:var(--suave);font-size:13px}
  .paginador{display:flex;align-items:center;gap:8px;margin:8px 0;flex-wrap:wrap}
  .paginador button{font-size:12px;padding:4px 8px}
  .vacio{padding:16px 0;color:var(--suave)}
  .distribucion{display:grid;grid-template-columns:minmax(0,1fr) minmax(230px,310px);gap:12px}
  .arbol{position:relative;overflow:auto;height:clamp(365px,62vh,660px);background-color:#181f2c;isolation:isolate;
      background-image:radial-gradient(#394357 1px,transparent 1px);background-size:21px 21px;cursor:grab;overscroll-behavior:contain}
  .arbol.arrastre{cursor:grabbing;user-select:none}
  #lienzo{position:relative;overflow:hidden}
  #diagrama{position:absolute;top:0;left:0;transform-origin:top left}
  .enlace{position:absolute;height:1px;background:#8c9eb7;transform-origin:left center;pointer-events:none}
  .nodo{position:absolute;height:54px}
  .ficha{display:flex;flex-direction:column;gap:2px;justify-content:center;align-items:center;overflow:hidden;
      width:100%;height:100%;padding:4px 7px;color:#edf3fd;background:#25344e;border:1px solid #88afe8;border-radius:11px}
  .ficha.hoja{background:#213a37;border-color:#68cfaa}
  .ficha.raiz{border-color:#e8ae61;background:#4e3928}
  .ficha.error{border-color:#ff899f}
  .ficha:hover,.ficha[aria-pressed=true]{background:#354d73}
  .ficha[aria-pressed=true]{box-shadow:0 0 0 2px #b5d2ff}
  .nodo-id{color:#cad7ef;font:10px/1.1 ui-monospace,monospace}
  .texto-nodo{width:100%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:13px;font-weight:650}
  .flecha{position:absolute;z-index:1;right:-6px;bottom:-9px;border-radius:100%;width:25px;height:25px;padding:0;
      background:#1b273a;color:#ecf4ff;border:1px solid #96acce;font-size:16px;line-height:1}
  .flecha:hover{background:#38527c}
  .detalle{padding:12px;overflow-wrap:anywhere;align-self:start}
  .detalle p{margin:5px 0 10px}
  .detalle pre{white-space:pre-wrap;overflow-wrap:anywhere;max-height:150px;overflow:auto}
  #navegar-ast{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-bottom:10px}
  #zoom-actual{min-width:49px;text-align:center}
  .navegacion-ramas{display:flex;gap:5px;flex-wrap:wrap;margin-top:10px}
  .ruta{display:flex;gap:4px;flex-wrap:wrap;margin-top:8px}
  .ruta button{font-size:11px;padding:2px 5px}
  #resultados-ast{max-height:190px;overflow:auto;margin-bottom:8px}
  #resultados-ast .resultado-ast{display:block;width:100%;text-align:left;margin-top:4px}
  @media(max-width:740px){.distribucion{grid-template-columns:minmax(0,1fr)}}
</style>
</head>
<body>
<header>
  <div class="encabezado">
    <p>ChocoPy</p><h1>Reporte de compilación</h1>
    <span class="archivo" id="archivo"></span><span class="estado" id="estado"></span>
  </div>
</header>
<main>
  <nav role="tablist" aria-label="Reporte">
    <button id="tab-seguimiento" role="tab" aria-controls="seguimiento" aria-selected="true" type="button">Seguimiento</button>
    <button id="tab-simbolos" role="tab" aria-controls="simbolos" aria-selected="false" tabindex="-1" type="button">Tabla de símbolos</button>
    <button id="tab-ast" role="tab" aria-controls="ast" aria-selected="false" tabindex="-1" type="button">AST</button>
    <button id="tab-errores" role="tab" aria-controls="errores" aria-selected="false" tabindex="-1" type="button">Errores<span id="total-errores"></span></button>
  </nav>
  <section id="seguimiento" role="tabpanel" aria-labelledby="tab-seguimiento">
    <div class="barra"><label for="fase">Fase</label>
      <select id="fase"><option value="">Todas</option><option value="Léxico">Léxica</option><option value="LR(1)">LR(1) y construcción del AST</option><option value="Semántico">Semántica</option></select>
      <label for="orden-traza">Orden</label>
      <select id="orden-traza"><option value="ejecucion">Ejecución</option><option value="linea">Línea</option></select>
    </div>
    <div id="tabla-seguimiento"></div>
  </section>
  <section id="simbolos" role="tabpanel" aria-labelledby="tab-simbolos" hidden>
    <p class="guia-traza" id="simbolos-parciales" hidden>Tabla parcial. Muestra los símbolos de las declaraciones recuperadas.</p>
    <div class="barra" id="elegir-ambito"><label for="ambito">Ámbito</label><select id="ambito"><option value="">Todos los ámbitos</option></select></div>
    <p class="meta" id="padre"></p><div id="tabla-simbolos"></div>
  </section>
  <section id="ast" role="tabpanel" aria-labelledby="tab-ast" hidden>
    <p class="vacio" id="ast-vacio">Esta ejecución no generó un AST.</p>
    <div id="explorador" hidden>
      <p class="guia-traza" id="ast-parcial" hidden>AST parcial. Los nodos de error identifican los fragmentos omitidos durante la recuperación.</p>
      <div class="barra">
        <label for="buscar-ast">Buscar</label>
        <input type="search" id="buscar-ast" placeholder="Nombre, tipo, n12, L1 o L1:3">
        <button id="plegar" type="button">Plegar ramas</button>
        <button id="volver" type="button" hidden>Volver al programa</button>
        <span class="meta" id="total-nodos"></span>
      </div>
      <div id="resultados-ast" hidden></div>
      <div id="navegar-ast">
        <button id="alejar" type="button" aria-label="Alejar">−</button>
        <span id="zoom-actual" class="meta" aria-live="polite"></span>
        <button id="acercar" type="button" aria-label="Acercar">+</button>
        <button id="ajustar" type="button">Ajustar vista</button>
        <span class="meta">Arrastra el fondo para moverte por el árbol.</span>
      </div>
      <div class="distribucion">
        <div class="arbol" id="arbol" tabindex="0" role="region" aria-label="Diagrama del AST">
          <div id="lienzo"><div id="diagrama"></div></div>
        </div>
        <aside class="detalle" id="detalle" aria-label="Detalle del nodo"></aside>
      </div>
    </div>
  </section>
  <section id="errores" role="tabpanel" aria-labelledby="tab-errores" hidden>
    <div id="tabla-errores"></div>
  </section>
</main>
<script type="application/json" id="datos">
)HTML";

constexpr char Fin_HTML[] = R"HTML(
</script>
<script>
'use strict';

let Datos = JSON.parse(document.getElementById('datos').textContent);
document.getElementById('datos').remove();

function Elemento(Id) {
  return document.getElementById(Id);
}

function Crear(Etiqueta, Texto = '', Clase = '') {
  let Control = document.createElement(Etiqueta);
  Control.textContent = Texto;
  Control.className = Clase;
  return Control;
}

function Boton(Texto, Accion, Clase = '') {
  let Control = Crear('button', Texto, Clase);
  Control.type = 'button';
  Control.addEventListener('click', Accion);
  return Control;
}

function Mostrar_Pestana(Nombre, Enfocar = false) {
  for (let Pestana of Pestanas) {
    let Id = Pestana.getAttribute('aria-controls');
    let Activa = Id === Nombre;
    Pestana.setAttribute('aria-selected', String(Activa));
    Pestana.tabIndex = Activa ? 0 : -1;
    Elemento(Id).hidden = !Activa;
    if (Activa && Enfocar) Pestana.focus();
  }
  if (Nombre === 'ast' && Raiz) Ajustar_Lienzo();
}

function Paginador(Total, Pagina, Tamano, Cambiar) {
  let Barra = Crear('div', '', 'paginador');
  let Inicio = Pagina * Tamano;
  let Fin = Math.min(Inicio + Tamano, Total);
  let Anterior = Boton('Anterior', () => Cambiar(Pagina - 1));
  let Siguiente = Boton('Siguiente', () => Cambiar(Pagina + 1));
  Anterior.disabled = Pagina === 0;
  Siguiente.disabled = Fin === Total;
  Barra.append(Anterior, Siguiente, Crear('span', (Inicio + 1) + '–' + Fin + ' de ' + Total, 'meta'));
  return Barra;
}

function Tabla(Destino, Encabezados, Filas, Obtener_Celdas, Vacio) {
  let Tamano = 75;

  function Dibujar(Pagina) {
    Destino.replaceChildren();
    if (!Filas.length) {
      Destino.append(Crear('p', Vacio, 'vacio'));
      return;
    }
    if (Filas.length > Tamano) {
      Destino.append(Paginador(Filas.length, Pagina, Tamano, Dibujar));
    }

    let Contenedor = Crear('div', '', 'tabla');
    let Tabla_HTML = Crear('table');
    let Cabecera = Tabla_HTML.createTHead().insertRow();
    for (let Nombre of Encabezados) {
      let Celda = Crear('th', Nombre);
      Celda.scope = 'col';
      Cabecera.append(Celda);
    }

    let Cuerpo = Tabla_HTML.createTBody();
    let Inicio = Pagina * Tamano;
    let Fin = Math.min(Inicio + Tamano, Filas.length);
    for (let I = Inicio; I < Fin; I++) {
      let Fila = Cuerpo.insertRow();
      for (let Valor of Obtener_Celdas(Filas[I])) {
        let Celda = Fila.insertCell();
        if (Valor instanceof Node) {
          Celda.append(Valor);
          if (Valor.dataset.celda) Celda.className = Valor.dataset.celda;
        } else Celda.textContent = Valor;
      }
    }
    Contenedor.append(Tabla_HTML);
    Destino.append(Contenedor);
  }

  Dibujar(0);
}

let Pestanas = Array.from(document.querySelectorAll('[role="tab"]'));
Pestanas.forEach((Pestana, Indice) => {
  Pestana.addEventListener('click', () => Mostrar_Pestana(Pestana.getAttribute('aria-controls')));
  Pestana.addEventListener('keydown', Evento => {
    let Siguiente;
    if (Evento.key === 'ArrowRight') Siguiente = (Indice + 1) % Pestanas.length;
    else if (Evento.key === 'ArrowLeft') Siguiente = (Indice + Pestanas.length - 1) % Pestanas.length;
    else if (Evento.key === 'Home') Siguiente = 0;
    else if (Evento.key === 'End') Siguiente = Pestanas.length - 1;
    else return;
    Evento.preventDefault();
    Mostrar_Pestana(Pestanas[Siguiente].getAttribute('aria-controls'), true);
  });
});

Elemento('archivo').textContent = Datos.Archivo;
Elemento('estado').textContent = Datos.Estado;
Elemento('estado').classList.toggle('error', Datos.Errores.length > 0);
Elemento('total-errores').textContent = ' (' + Datos.Errores.length + ')';

function Dibujar_Seguimiento() {
  let Fase = Elemento('fase').value;
  let Indices = [];
  for (let I = 0; I < Datos.Pasos.length; I++) {
    if (!Fase || Datos.Pasos[I][0] === Fase) Indices.push(I);
  }
  if (Elemento('orden-traza').value === 'linea') {
    let Linea = I => Number(Datos.Pasos[I][8]) || Number.MAX_SAFE_INTEGER;
    Indices.sort((A, B) => Linea(A) - Linea(B) || A - B);
  }
  Tabla(Elemento('tabla-seguimiento'), ['Paso', 'Línea', 'Fase', 'Estados / ámbito', 'Símbolos / nodo', 'ATTR', 'Entrada', 'Acción'],
    Indices, I => {
      let [Fase, Estados, Simbolos, Entrada, Accion, Atributos, Accion_AST, Nodo, Linea] = Datos.Pasos[I];
      let Celda_Accion = Crear('div');
      Celda_Accion.append(Crear('span', Accion, 'paso-accion'));
      let Id = Number(Nodo);
      if (Datos.Raiz && Datos.Nodos[Id]) {
        Celda_Accion.dataset.celda = 'accion-con-nodo';
        Celda_Accion.append(Boton(Accion_AST || 'Ver n' + Id, () => Ir_Al_AST(Id), 'accion-ast'));
      } else if (Accion_AST) {
        Celda_Accion.append(Crear('span', Accion_AST, 'accion-ast'));
      }
      return [I + 1, Linea || '—', Fase, Estados, Simbolos, Atributos, Entrada, Celda_Accion];
    }, 'No hay pasos registrados para esta fase.');
}

function Dibujar_Simbolos() {
  let Destino = Elemento('tabla-simbolos');
  if (!Datos.Ambitos.length) {
    Elemento('elegir-ambito').hidden = true;
    Destino.replaceChildren(Crear('p', 'Esta ejecución no generó una tabla de símbolos.', 'vacio'));
    return;
  }

  let Selector = Elemento('ambito');
  let General = Selector.value === '';
  let Indice_Actual = Number(Selector.value);
  let Ambito_Actual = Datos.Ambitos[Indice_Actual];
  let Relaciones = [];
  if (!General) {
    if (Ambito_Actual.Padre) Relaciones.push('Ámbito padre: ' + Ambito_Actual.Padre);
    if (Ambito_Actual.Superclase) Relaciones.push('Superclase: ' + Ambito_Actual.Superclase);
  }
  Elemento('padre').textContent = Relaciones.join(' · ');
  Elemento('padre').hidden = !Relaciones.length;

  let Filas = [];
  for (let [Indice, Ambito] of Datos.Ambitos.entries()) {
    if (!General && Indice !== Indice_Actual) continue;
    for (let Simbolo of Ambito.Simbolos) Filas.push([Indice, Simbolo]);
  }

  Tabla(Destino, ['Ámbito', 'Nombre', 'Categoría', 'Tipo o firma', 'Declaración', 'Origen'], Filas, ([Indice, Simbolo]) => {
    let Nombre_Ambito = Datos.Ambitos[Indice].Nombre;
    let Celda_Ambito = General ? Boton(Nombre_Ambito, () => {
      Selector.value = String(Indice);
      Selector.focus();
      Dibujar_Simbolos();
    }, 'enlace-ambito') : Nombre_Ambito;
    return [Celda_Ambito, ...Simbolo];
  }, General ? 'No hay símbolos registrados.' : 'Este ámbito no tiene declaraciones propias.');
}

function Dibujar_Errores() {
  Tabla(Elemento('tabla-errores'), ['Fase', 'Línea', 'Columna', 'Causa', 'AST'], Datos.Errores,
    Error => [Error.Fase, Error.Linea || '—', Error.Columna || '—', Error.Mensaje,
      Error.Nodo ? Boton('Ver nodo', () => Ir_Al_AST(Error.Nodo)) : '—'], 'No se encontraron errores.');
}

let Nodos = Datos.Nodos;
let Raiz = Datos.Raiz;
let Cantidad_Nodos = 0;
let Raiz_Visible = Raiz;
let Seleccion = Raiz;
let Padres = [];
let Indices_Hijos = [];
let Ramas = new Map();
let Dibujados = new Map();
let Tamano_Rama = 10;
let Limite_Ramas = 24;
let Ancho_Nodo = 134;
let Alto_Nodo = 54;
let Paso_X = 160;
let Paso_Y = 105;
let Margen = 24;
let Ancho_Dibujo = 0;
let Alto_Dibujo = 0;
let Zoom = 1;

function Ubicacion(Nodo) {
  return 'L' + Nodo.Linea + ':' + Nodo.Columna;
}

function Titulo(Id) {
  let Nodo = Nodos[Id];
  return Nodo.Texto ? Nodo.Clase + ' ' + Nodo.Texto : Nodo.Clase;
}

function Acortar(Texto, Limite = 100) {
  return Texto.length > Limite ? Texto.slice(0, Limite) + '…' : Texto;
}

function Ir_Al_AST(Id) {
  Mostrar_Pestana('ast', true);
  Mostrar_Nodo(Id);
}

function Seleccionar(Id) {
  let Anterior = Dibujados.get(Seleccion);
  if (Anterior) Anterior.Ficha.setAttribute('aria-pressed', 'false');
  Seleccion = Id;
  Dibujados.get(Id).Ficha.setAttribute('aria-pressed', 'true');

  let Nodo = Nodos[Id];
  let Detalle = Elemento('detalle');
  Detalle.replaceChildren(Crear('p', 'n' + Id + ' · ' + Ubicacion(Nodo) + ' · ' + Nodo.Hijos.length + ' hijos', 'meta'),
                          Crear('h3', Nodo.Clase));
  if (Nodo.Texto) Detalle.append(Crear('pre', Nodo.Texto, 'mono'));
  if (Nodo.Tipo) Detalle.append(Crear('p', 'Tipo: ' + Nodo.Tipo, 'mono'));

  let Padre = Padres[Id];
  if (Padre) {
    let Rol = Nodos[Padre].Hijos[Indices_Hijos[Id]][1];
    if (Rol) Detalle.append(Crear('p', 'Rol: ' + Rol, 'meta'));
  }
  if (Nodo.Hijos.length) {
    let Controles = Crear('div', '', 'navegacion-ramas');
    Controles.append(Boton(Ramas.has(Id) ? 'Plegar hijos' : 'Desplegar hijos', () => Alternar_Rama(Id)));
    if (Id !== Raiz_Visible) Controles.append(Boton('Enfocar rama', () => Reiniciar_AST(Id)));
    Detalle.append(Controles);
    if (Ramas.has(Id) && Nodo.Hijos.length > Tamano_Rama) {
      Detalle.append(Paginador(Nodo.Hijos.length, Ramas.get(Id), Tamano_Rama, Pagina => Cambiar_Pagina(Id, Pagina)));
    }
  }

  let Ruta = Crear('div', '', 'ruta');
  for (let Ancestro = Padre; Ancestro && Ruta.childElementCount < 6; Ancestro = Padres[Ancestro]) {
    let Destino = Ancestro;
    Ruta.prepend(Boton(Acortar(Titulo(Ancestro), 28), () => Mostrar_Nodo(Destino)));
  }
  Detalle.append(Ruta);
}

function Cerrar_Descendientes(Id) {
  for (let Rama of Ramas.keys()) {
    for (let Padre = Padres[Rama]; Padre; Padre = Padres[Padre]) {
      if (Padre === Id) {
        Ramas.delete(Rama);
        break;
      }
    }
  }
}

function Alternar_Rama(Id) {
  if (Ramas.has(Id)) {
    Cerrar_Descendientes(Id);
    Ramas.delete(Id);
  } else {
    if (Ramas.size >= Limite_Ramas) {
      Reiniciar_AST(Id);
      return;
    }
    Ramas.set(Id, 0);
  }
  Dibujar_AST(Id);
}

function Cambiar_Pagina(Id, Pagina) {
  Cerrar_Descendientes(Id);
  Ramas.set(Id, Pagina);
  Dibujar_AST(Id);
}

function Dibujar_Enlace(Padre, Hijo) {
  let X = Hijo.X - Padre.X;
  let Y = Hijo.Y - Padre.Y - Alto_Nodo;
  let Linea = Crear('div', '', 'enlace');
  Linea.setAttribute('aria-hidden', 'true');
  Linea.style.left = Padre.X + 'px';
  Linea.style.top = (Padre.Y + Alto_Nodo) + 'px';
  Linea.style.width = Math.hypot(X, Y) + 'px';
  Linea.style.transform = 'rotate(' + Math.atan2(Y, X) + 'rad)';
  return Linea;
}

function Dibujar_Nodo(Parte) {
  let Id = Parte.Id;
  let Nodo = Nodos[Id];
  let Caja = Crear('div', '', 'nodo');
  Caja.dataset.nodo = String(Id);
  Caja.style.left = (Parte.X - Ancho_Nodo / 2) + 'px';
  Caja.style.top = Parte.Y + 'px';
  Caja.style.width = Ancho_Nodo + 'px';
  let Clase = Id === Raiz_Visible ? ' raiz' : Nodo.Hijos.length ? '' : ' hoja';
  let Ficha = Boton('', () => Seleccionar(Id), 'ficha' + Clase + (Nodo.Error ? ' error' : ''));
  Ficha.title = Titulo(Id) + ' · ' + Ubicacion(Nodo) + (Nodo.Tipo ? ' · ' + Nodo.Tipo : '');
  Ficha.setAttribute('aria-label', 'n' + Id + ' ' + Titulo(Id));
  Ficha.setAttribute('aria-pressed', 'false');
  Ficha.append(Crear('span', 'n' + Id, 'nodo-id'), Crear('span', Acortar(Nodo.Texto || Nodo.Clase, 36), 'texto-nodo'));
  Caja.append(Ficha);
  Parte.Ficha = Ficha;

  if (Nodo.Hijos.length) {
    let Abierta = Ramas.has(Id);
    let Control = Boton(Abierta ? '−' : '+', () => Alternar_Rama(Id), 'flecha');
    Control.setAttribute('aria-expanded', String(Abierta));
    Control.setAttribute('aria-label', (Abierta ? 'Plegar' : 'Desplegar') + ' n' + Id + ' (' + Nodo.Hijos.length + ' hijos)');
    Caja.append(Control);
  }
  return Caja;
}

function Dibujar_AST(Id = Raiz_Visible) {
  let Partes = [{Id: Raiz_Visible, X: 0, Y: Margen, Inicio: Margen, Ancho: Paso_X, Hijos: []}];
  for (let Parte of Partes) {
    if (!Ramas.has(Parte.Id)) continue;
    let Inicio = Ramas.get(Parte.Id) * Tamano_Rama;
    for (let [Hijo] of Nodos[Parte.Id].Hijos.slice(Inicio, Inicio + Tamano_Rama)) {
      let Hijo_Parte = {Id: Hijo, X: 0, Y: Parte.Y + Paso_Y, Inicio: 0, Ancho: Paso_X, Hijos: []};
      Parte.Hijos.push(Hijo_Parte);
      Partes.push(Hijo_Parte);
    }
  }

  // Reserva un espacio horizontal por subárbol y centra cada padre sobre sus hijos visibles.
  for (let I = Partes.length - 1; I >= 0; I--) {
    let Parte = Partes[I];
    if (Parte.Hijos.length) Parte.Ancho = Parte.Hijos.reduce((Suma, Hijo) => Suma + Hijo.Ancho, 0);
  }
  for (let Parte of Partes) {
    Parte.X = Parte.Inicio + Parte.Ancho / 2;
    let Inicio = Parte.Inicio;
    for (let Hijo of Parte.Hijos) {
      Hijo.Inicio = Inicio;
      Inicio += Hijo.Ancho;
    }
  }

  let Diagrama = Elemento('diagrama');
  Diagrama.replaceChildren();
  Dibujados.clear();
  for (let Parte of Partes) {
    for (let Hijo of Parte.Hijos) Diagrama.append(Dibujar_Enlace(Parte, Hijo));
  }
  for (let Parte of Partes) {
    Dibujados.set(Parte.Id, Parte);
    Diagrama.append(Dibujar_Nodo(Parte));
  }

  Ancho_Dibujo = Partes[0].Ancho + Margen * 2;
  Alto_Dibujo = Partes[Partes.length - 1].Y + Alto_Nodo + Margen;
  Diagrama.style.width = Ancho_Dibujo + 'px';
  Diagrama.style.height = Alto_Dibujo + 'px';
  Seleccionar(Dibujados.has(Id) ? Id : Raiz_Visible);
  Ajustar_Lienzo();
}

function Ajustar_Lienzo() {
  let Vista = Elemento('arbol');
  let Lienzo = Elemento('lienzo');
  let Diagrama = Elemento('diagrama');
  let Margen_Centrado = Math.max(0, (Vista.clientWidth - Ancho_Dibujo * Zoom) / 2);
  Lienzo.style.width = Math.max(Vista.clientWidth, Ancho_Dibujo * Zoom) + 'px';
  Lienzo.style.height = Math.max(Vista.clientHeight, Alto_Dibujo * Zoom) + 'px';
  Diagrama.style.transform = 'translateX(' + Margen_Centrado + 'px) scale(' + Zoom + ')';
  Elemento('zoom-actual').textContent = Math.round(Zoom * 100) + '%';

  let Parte = Dibujados.get(Seleccion);
  if (Parte) {
    Vista.scrollLeft = Math.max(0, Math.round(Margen_Centrado + Parte.X * Zoom - Vista.clientWidth / 2));
    Vista.scrollTop = Math.max(0, Math.round((Parte.Y + Alto_Nodo / 2) * Zoom - Vista.clientHeight / 2));
  }
}

function Cambiar_Zoom(Nuevo) {
  Zoom = Math.min(2, Math.max(.3, Nuevo));
  Ajustar_Lienzo();
}

function Reiniciar_AST(Id, Vista_Inicial = false) {
  Raiz_Visible = Id;
  Elemento('volver').hidden = Id === Raiz;
  Ramas.clear();
  if (Nodos[Id].Hijos.length) Ramas.set(Id, 0);
  if (Vista_Inicial && Cantidad_Nodos < 45) {
    for (let I = 1; I < Nodos.length && Ramas.size < Limite_Ramas; I++) {
      if (Nodos[I] && Nodos[I].Hijos.length) Ramas.set(I, 0);
    }
  } else if (Vista_Inicial && Nodos[Id].Hijos.length <= 5) {
    for (let [Hijo] of Nodos[Id].Hijos) {
      if (Nodos[Hijo].Hijos.length && Nodos[Hijo].Hijos.length <= 6) Ramas.set(Hijo, 0);
    }
  }
  Dibujar_AST(Id);
}

function Mostrar_Nodo(Id) {
  if (Dibujados.has(Id)) {
    Seleccionar(Id);
    Ajustar_Lienzo();
    return;
  }

  let Camino = [];
  let Actual = Id;
  while (Actual !== Raiz_Visible && Padres[Actual]) {
    Camino.push(Actual);
    Actual = Padres[Actual];
  }
  if (Actual !== Raiz_Visible || Camino.length > 6 || Camino.length + Ramas.size > Limite_Ramas) {
    Raiz_Visible = Camino.length > 6 ? Camino[4] : Raiz;
    Elemento('volver').hidden = Raiz_Visible === Raiz;
    Ramas.clear();
    if (Nodos[Raiz_Visible].Hijos.length) Ramas.set(Raiz_Visible, 0);
  }

  let Limite = Camino.indexOf(Raiz_Visible);
  if (Limite < 0) Limite = Camino.length;
  for (let I = Limite - 1; I >= 0; I--) {
    let Hijo = Camino[I];
    let Padre = Padres[Hijo];
    let Pagina = Math.floor(Indices_Hijos[Hijo] / Tamano_Rama);
    if (Ramas.get(Padre) !== Pagina) Cerrar_Descendientes(Padre);
    Ramas.set(Padre, Pagina);
  }
  Dibujar_AST(Id);
}

function Habilitar_Arrastre() {
  let Vista = Elemento('arbol');
  let Arrastre;
  Vista.addEventListener('pointerdown', Evento => {
    if (Evento.pointerType === 'touch' || Evento.button !== 0 || Evento.target.closest('button')) return;
    Arrastre = {X: Evento.clientX, Y: Evento.clientY, Izquierda: Vista.scrollLeft, Arriba: Vista.scrollTop};
    Vista.classList.add('arrastre');
    Vista.setPointerCapture(Evento.pointerId);
  });
  Vista.addEventListener('pointermove', Evento => {
    if (!Arrastre) return;
    Vista.scrollLeft = Arrastre.Izquierda + Arrastre.X - Evento.clientX;
    Vista.scrollTop = Arrastre.Arriba + Arrastre.Y - Evento.clientY;
  });
  Vista.addEventListener('lostpointercapture', () => {
    Arrastre = undefined;
    Vista.classList.remove('arrastre');
  });
}

function Normalizar_Busqueda(Texto) {
  return Texto.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g, '');
}

function Buscar_AST() {
  let Consulta = Normalizar_Busqueda(Elemento('buscar-ast').value.trim());
  let Resultados = Elemento('resultados-ast');
  Resultados.replaceChildren();
  Resultados.hidden = !Consulta;
  if (!Consulta) return 0;

  let Por_Id = /^n(\d+)$/.exec(Consulta);
  let Posicion = /^(?:l|@)(\d+)(?::(\d+))?$/.exec(Consulta);
  let Linea = Posicion ? Number(Posicion[1]) : 0;
  let Columna = Posicion && Posicion[2] ? Number(Posicion[2]) : 0;
  let Coincidencias = [];
  for (let Id = 1; Id < Nodos.length; Id++) {
    let Nodo = Nodos[Id];
    if (!Nodo) continue;
    let Coincide = Por_Id ? Id === Number(Por_Id[1])
                           : Posicion ? Nodo.Linea === Linea && (!Posicion[2] || Nodo.Columna === Columna)
                           : Normalizar_Busqueda(Titulo(Id) + ' ' + Nodo.Tipo).includes(Consulta);
    if (Coincide) Coincidencias.push(Id);
  }
  if (Posicion) Coincidencias.sort((A, B) => Nodos[A].Columna - Nodos[B].Columna);

  let Tamano = 25;
  function Dibujar_Resultados(Pagina) {
    Resultados.replaceChildren();
    Resultados.scrollTop = 0;
    if (Coincidencias.length > Tamano) {
      Resultados.append(Paginador(Coincidencias.length, Pagina, Tamano, Dibujar_Resultados));
    } else {
      Resultados.append(Crear('div', Coincidencias.length + ' coincidencias', 'meta'));
    }

    let Inicio = Pagina * Tamano;
    for (let Id of Coincidencias.slice(Inicio, Inicio + Tamano)) {
      let Etiqueta = Ubicacion(Nodos[Id]) + ' · n' + Id + ' · ' + Acortar(Titulo(Id));
      Resultados.append(Boton(Etiqueta, () => Mostrar_Nodo(Id), 'resultado-ast'));
    }
  }
  Dibujar_Resultados(0);
  return Coincidencias[0] || 0;
}

if (Raiz) {
  for (let Id = 1; Id < Nodos.length; Id++) {
    if (!Nodos[Id]) continue;
    Cantidad_Nodos++;
    Nodos[Id].Hijos.forEach(([Hijo], Posicion) => {
      Padres[Hijo] = Id;
      Indices_Hijos[Hijo] = Posicion;
    });
  }

  Elemento('ast-vacio').hidden = true;
  Elemento('explorador').hidden = false;
  Elemento('ast-parcial').hidden = !Datos.Parcial;
  Elemento('simbolos-parciales').hidden = !Datos.Parcial;
  Elemento('total-nodos').textContent = Cantidad_Nodos.toLocaleString('es-PE') + ' nodos';
  Elemento('plegar').addEventListener('click', () => Reiniciar_AST(Raiz_Visible));
  Elemento('volver').addEventListener('click', () => Reiniciar_AST(Raiz));
  Elemento('buscar-ast').addEventListener('input', Buscar_AST);
  Elemento('buscar-ast').addEventListener('keydown', Evento => {
    if (Evento.key !== 'Enter') return;
    let Id = Buscar_AST();
    if (Id) Mostrar_Nodo(Id);
  });
  Elemento('acercar').addEventListener('click', () => Cambiar_Zoom(Zoom * 1.2));
  Elemento('alejar').addEventListener('click', () => Cambiar_Zoom(Zoom / 1.2));
  Elemento('ajustar').addEventListener('click', () => {
    let Vista = Elemento('arbol');
    Cambiar_Zoom(Math.min(1, Vista.clientWidth / Ancho_Dibujo, Vista.clientHeight / Alto_Dibujo));
  });
  Habilitar_Arrastre();
  Reiniciar_AST(Raiz, true);
}

Datos.Ambitos.forEach((Ambito, Indice) => {
  let Opcion = Crear('option', Ambito.Nombre);
  Opcion.value = Indice;
  Elemento('ambito').append(Opcion);
});
Elemento('fase').addEventListener('change', Dibujar_Seguimiento);
Elemento('orden-traza').addEventListener('change', Dibujar_Seguimiento);
Elemento('ambito').addEventListener('change', Dibujar_Simbolos);
Dibujar_Seguimiento();
Dibujar_Simbolos();
Dibujar_Errores();
</script>
</body>
</html>
)HTML";

} // namespace

void Guardar_Reporte(const std::filesystem::path& Ruta, std::string_view Archivo, std::string_view Estado,
                     AST& Arbol, Seguimiento& Traza, std::vector<Tabla_Ambito>& Tablas) {

    namespace fs = std::filesystem;
    fs::path Temporal = Ruta;
    Temporal += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    try {
        std::ofstream Salida(Temporal, std::ios::binary);
        if (!Salida) {
            throw std::runtime_error("No se pudo crear el reporte en '" + Ruta.string() + "'");
        }

        Salida << Inicio_HTML << "{\"Archivo\":";
        Escribir_Texto(Salida, Archivo);
        Salida << ",\"Estado\":";
        Escribir_Texto(Salida, Estado);
        Escribir_Seguimiento(Salida, Traza);
        Escribir_Tablas(Salida, Tablas);
        auto Visibles = Nodos_Visibles(Arbol);
        Escribir_AST(Salida, Arbol, Visibles);
        Escribir_Errores(Salida, Traza, Visibles);
        Salida << '}' << Fin_HTML;

        Salida.close();
        if (!Salida) {
            throw std::runtime_error("No se pudo guardar el reporte en '" + Ruta.string() + "'");
        }
        fs::rename(Temporal, Ruta);
    } catch (...) {
        std::error_code Ignorar;
        fs::remove(Temporal, Ignorar);
        throw;
    }
}
