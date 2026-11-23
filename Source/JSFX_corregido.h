#pragma once
#include <JuceHeader.h>
#include <map>
#include <memory>
#include <vector>
#include <cmath>
#include <cctype>
#include <cstring>
#include <string>

using namespace juce;

// ===========================================================================
// JSFX para La Pauta Studio
//
//  1) ANALIZADOR  (jsfx::compileFile):  lee un .jsfx como los de REAPER y detecta nombre, descripción,
//                 sliders (nombre, mínimo, máximo, valor por defecto), entradas y salidas de audio.
//  2) INTÉRPRETE  (jsfx::Program):      traduce el código EEL2 de @init / @slider / @block / @sample
//                 a un árbol de nodos y lo ejecuta muestra a muestra. Si hay un error de sintaxis se lanza
//                 jsfx::Error con el número de línea (nunca se cae el programa).
//  3) PLUGIN      (JsfxProcessor):      un AudioPluginInstance normal: entra en la cadena de FX de una pista
//                 como cualquier VST3, sus sliders son parámetros (se pueden mover y automatizar).
//
// Soportado: variables, operadores de EEL2, if/else, ?:, loop, while, funciones propias (function ... local()
// instance() y namespaces obj.fn()), memoria mem[]/gmem[], spl0..spl63, slider1..slider64, srate, samplesblock,
// num_ch, tempo, y las funciones matemáticas habituales. No soportado: @gfx (gráficos), @serialize, import,
// cadenas de texto, fft/mdct, MIDI.
// ===========================================================================
namespace jsfx
{
inline String tr (const char* utf8) { return String::fromUTF8 (utf8); }

struct Error
{
    int line = 0;
    String msg;
};

// ---------------------------------------------------------------------------
// Resultado del análisis de la cabecera
// ---------------------------------------------------------------------------
struct SliderInfo
{
    int index = 0;                 // sliderN (1..64)
    String name;
    String varName;                // sliderN:miVariable=valor<...>
    double minV = 0.0, maxV = 1.0, defV = 0.0, step = 0.0;
    StringArray enumNames;         // slider1:0<0,2,1{Apagado,Suave,Fuerte}>nombre
    bool hidden = false;           // nombre que empieza con '-' o slider de archivo
    int varIdx = -1;               // variable extra (varName) dentro del intérprete
};

struct Info
{
    String file, name, desc, tags;
    std::vector<SliderInfo> sliders;
    StringArray inPins, outPins;
    bool pinsDeclared = false;
    StringArray warnings;

    int numIn() const  { return pinsDeclared ? inPins.size()  : 2; }
    int numOut() const { return pinsDeclared ? outPins.size() : 2; }

    static String num (double v)
    {
        String s (v, 4);
        if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s.isEmpty() ? String ("0") : s;
    }

    // Texto del análisis (se muestra en el editor del efecto)
    String report() const
    {
        String s;
        s << tr ("Archivo: ") << File (file).getFileName() << "\n";
        s << tr ("Nombre del efecto: ") << name << "\n";
        s << tr ("Descripción: ") << (desc.isNotEmpty() ? desc : tr ("(sin desc:)")) << "\n";
        if (tags.isNotEmpty()) s << tr ("Etiquetas: ") << tags << "\n";

        auto pins = [] (bool declared, const StringArray& p) -> String
        {
            if (! declared) return tr ("2 (no declaradas: usa los canales de la pista)");
            return String (p.size()) + (p.isEmpty() ? String() : " (" + p.joinIntoString (", ") + ")");
        };
        s << tr ("Entradas de audio: ") << pins (pinsDeclared, inPins) << "\n";
        s << tr ("Salidas de audio: ") << pins (pinsDeclared, outPins) << "\n";

        s << tr ("Sliders (") << String ((int) sliders.size()) << "):\n";
        for (auto& sl : sliders)
        {
            s << "  " << String (sl.index) << ". " << sl.name << "   min " << num (sl.minV) << "   max " << num (sl.maxV)
              << "   def " << num (sl.defV);
            if (sl.step > 0.0) s << "   paso " << num (sl.step);
            if (! sl.enumNames.isEmpty()) s << "   {" << sl.enumNames.joinIntoString (", ") << "}";
            if (sl.hidden) s << tr ("   (oculto)");
            s << "\n";
        }
        if (sliders.empty()) s << tr ("  (este efecto no tiene sliders)\n");

        if (! warnings.isEmpty())
        {
            s << tr ("Avisos:\n");
            for (auto& w : warnings) s << "  - " << w << "\n";
        }
        return s;
    }
};

// ---------------------------------------------------------------------------
// Lexer (EEL2)
// ---------------------------------------------------------------------------
enum { tkEnd = 0, tkNum, tkId, tkOp };

struct Token
{
    int type = tkEnd;
    String text;
    double num = 0.0;
    int line = 0;
};

inline int hexVal (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    return std::tolower ((unsigned char) c) - 'a' + 10;
}

inline void tokenize (const String& source, int firstLine, std::vector<Token>& out)
{
    const std::string s = source.toStdString();
    const size_t len = s.size();
    size_t i = 0;
    int line = firstLine;

    auto isIdStart = [] (char c) { return std::isalpha ((unsigned char) c) || c == '_'; };
    auto isIdChar  = [] (char c) { return std::isalnum ((unsigned char) c) || c == '_' || c == '.'; };

    static const char* ops3[] = { "===", "!==" };
    static const char* ops2[] = { "==", "!=", "<=", ">=", "<<", ">>", "&&", "||", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "~=", "^=" };

    while (i < len)
    {
        const char c = s[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }

        if (c == '/' && i + 1 < len && s[i + 1] == '/')
        {
            while (i < len && s[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < len && s[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < len && ! (s[i] == '*' && s[i + 1] == '/')) { if (s[i] == '\n') ++line; ++i; }
            i += 2;
            continue;
        }

        Token t;
        t.line = line;

        if (std::isdigit ((unsigned char) c) || (c == '.' && i + 1 < len && std::isdigit ((unsigned char) s[i + 1])))
        {
            t.type = tkNum;
            t.text = "num";
            if (c == '0' && i + 1 < len && (s[i + 1] == 'x' || s[i + 1] == 'X'))
            {
                size_t j = i + 2;
                double v = 0.0;
                while (j < len && std::isxdigit ((unsigned char) s[j])) { v = v * 16.0 + hexVal (s[j]); ++j; }
                t.num = v;
                i = j;
            }
            else
            {
                char* end = nullptr;
                t.num = std::strtod (s.c_str() + i, &end);
                i = (size_t) (end - s.c_str());
            }
            out.push_back (t);
            continue;
        }

        if (isIdStart (c))
        {
            size_t j = i;
            while (j < len && isIdChar (s[j])) ++j;
            t.type = tkId;
            t.text = String (s.substr (i, j - i)).toLowerCase();
            i = j;
            out.push_back (t);
            continue;
        }

        if (c == '$')       // constantes: $pi, $e, $phi, $'c', $xFF
        {
            t.type = tkNum;
            t.text = "num";
            if (i + 3 < len && s[i + 1] == '\'' && s[i + 3] == '\'')
            {
                t.num = (double) (unsigned char) s[i + 2];
                i += 4;
            }
            else if (i + 1 < len && (s[i + 1] == 'x' || s[i + 1] == 'X'))
            {
                size_t j = i + 2;
                double v = 0.0;
                while (j < len && std::isxdigit ((unsigned char) s[j])) { v = v * 16.0 + hexVal (s[j]); ++j; }
                t.num = v;
                i = j;
            }
            else
            {
                size_t j = i + 1;
                while (j < len && std::isalpha ((unsigned char) s[j])) ++j;
                const String w = String (s.substr (i + 1, j - i - 1)).toLowerCase();
                if (w == "pi")       t.num = 3.14159265358979323846;
                else if (w == "e")   t.num = 2.71828182845904523536;
                else if (w == "phi") t.num = 1.61803398874989484820;
                else throw Error { line, tr ("constante desconocida: $") + w };
                i = j;
            }
            out.push_back (t);
            continue;
        }

        if (c == '"' || c == '#')
            throw Error { line, tr ("las cadenas de texto (\"...\" o #nombre) no están soportadas en este analizador") };

        t.type = tkOp;
        bool found = false;
        for (auto* o : ops3)
            if (s.compare (i, 3, o) == 0) { t.text = o; i += 3; found = true; break; }
        if (! found)
            for (auto* o : ops2)
                if (s.compare (i, 2, o) == 0) { t.text = o; i += 2; found = true; break; }
        if (! found)
        {
            if (c != 0 && std::strchr ("+-*/%^=<>!&|~?:;,()[]", c) != nullptr)
            {
                t.text = String::charToString ((juce_wchar) (unsigned char) c);
                ++i;
            }
            else
                throw Error { line, tr ("carácter inesperado: '") + String::charToString ((juce_wchar) (unsigned char) c) + "'" };
        }
        out.push_back (t);
    }
}

// ---------------------------------------------------------------------------
// Contexto de ejecución y nodos del árbol
// ---------------------------------------------------------------------------
struct Ctx
{
    double* vars = nullptr;
    double* mem = nullptr;     int memSize = 0;
    double* gmem = nullptr;    int gmemSize = 0;
    double* stack = nullptr;   int stackSize = 0;
    int sp = 0, fp = 0, depth = 0;
    int64 steps = 0, maxSteps = 0;
    bool aborted = false;
    uint32 rng = 22222u;
    double sink = 0.0;         // las escrituras fuera de rango caen aquí
};

enum Op { opAdd, opSub, opMul, opDiv, opMod, opPow, opShl, opShr, opOr, opAnd, opXor,
          opEq, opEqX, opNe, opNeX, opLt, opGt, opLe, opGe };

inline bool truthy (double v) { return std::abs (v) > 0.00001; }

inline int64 toI64 (double v)
{
    if (! (v > -9.0e18 && v < 9.0e18)) return 0;
    return (int64) v;
}

inline double applyBin (int op, double a, double b)
{
    switch (op)
    {
        case opAdd:  return a + b;
        case opSub:  return a - b;
        case opMul:  return a * b;
        case opDiv:  return b != 0.0 ? a / b : 0.0;
        case opMod:  { const int64 bi = toI64 (b); return bi != 0 ? (double) (toI64 (a) % bi) : 0.0; }
        case opPow:  return std::pow (a, b);
        case opShl:  return (double) (toI64 (a) << (toI64 (b) & 31));
        case opShr:  return (double) (toI64 (a) >> (toI64 (b) & 31));
        case opOr:   return (double) (toI64 (a) | toI64 (b));
        case opAnd:  return (double) (toI64 (a) & toI64 (b));
        case opXor:  return (double) (toI64 (a) ^ toI64 (b));
        case opEq:   return std::abs (a - b) < 0.00001 ? 1.0 : 0.0;
        case opEqX:  return a == b ? 1.0 : 0.0;
        case opNe:   return std::abs (a - b) < 0.00001 ? 0.0 : 1.0;
        case opNeX:  return a != b ? 1.0 : 0.0;
        case opLt:   return a < b ? 1.0 : 0.0;
        case opGt:   return a > b ? 1.0 : 0.0;
        case opLe:   return a <= b ? 1.0 : 0.0;
        case opGe:   return a >= b ? 1.0 : 0.0;
        default:     return 0.0;
    }
}

struct Node
{
    virtual ~Node() = default;
    virtual double eval (Ctx&) const = 0;
    virtual bool isLvalue() const { return false; }
    virtual double* slot (Ctx&) const { return nullptr; }
};
using NP = std::unique_ptr<Node>;

struct Num : Node
{
    double v;
    explicit Num (double x) : v (x) {}
    double eval (Ctx&) const override { return v; }
};

struct GVar : Node
{
    int idx;
    explicit GVar (int i) : idx (i) {}
    double eval (Ctx& c) const override { return c.vars[idx]; }
    bool isLvalue() const override { return true; }
    double* slot (Ctx& c) const override { return &c.vars[idx]; }
};

struct LVar : Node
{
    int s;
    explicit LVar (int i) : s (i) {}
    double eval (Ctx& c) const override { return c.stack[c.fp + s]; }
    bool isLvalue() const override { return true; }
    double* slot (Ctx& c) const override { return &c.stack[c.fp + s]; }
};

// mem[idx], gmem[idx] y base[idx] (= mem[base + idx])
struct MemIdx : Node
{
    NP base, idx;
    bool global = false;

    double* slot (Ctx& c) const override
    {
        const double a = (base ? base->eval (c) : 0.0) + (idx ? idx->eval (c) : 0.0);
        double* m = global ? c.gmem : c.mem;
        const int size = global ? c.gmemSize : c.memSize;
        const double fa = std::floor (a + 0.00001);
        if (fa >= 0.0 && fa < (double) size) return &m[(int) fa];
        c.sink = 0.0;
        return &c.sink;
    }
    double eval (Ctx& c) const override { return *slot (c); }
    bool isLvalue() const override { return true; }
};

// spl(n) y slider(n)
struct IdxVar : Node
{
    NP idx;
    int base = 0, count = 64, offset = 0;      // offset: slider(1) es la primera
    double* slot (Ctx& c) const override
    {
        int i = (int) toI64 (idx->eval (c)) - offset;
        i = jlimit (0, count - 1, i);
        return &c.vars[base + i];
    }
    double eval (Ctx& c) const override { return *slot (c); }
    bool isLvalue() const override { return true; }
};

struct Un : Node
{
    int op;      // 0 = menos, 1 = no lógico
    NP a;
    Un (int o, NP x) : op (o), a (std::move (x)) {}
    double eval (Ctx& c) const override
    {
        const double v = a->eval (c);
        return op == 0 ? -v : (truthy (v) ? 0.0 : 1.0);
    }
};

struct Bin : Node
{
    int op;
    NP a, b;
    Bin (int o, NP x, NP y) : op (o), a (std::move (x)), b (std::move (y)) {}
    double eval (Ctx& c) const override
    {
        const double x = a->eval (c);
        const double y = b->eval (c);
        return applyBin (op, x, y);
    }
};

struct AndN : Node
{
    NP a, b;
    AndN (NP x, NP y) : a (std::move (x)), b (std::move (y)) {}
    double eval (Ctx& c) const override { return (truthy (a->eval (c)) && truthy (b->eval (c))) ? 1.0 : 0.0; }
};

struct OrN : Node
{
    NP a, b;
    OrN (NP x, NP y) : a (std::move (x)), b (std::move (y)) {}
    double eval (Ctx& c) const override { return (truthy (a->eval (c)) || truthy (b->eval (c))) ? 1.0 : 0.0; }
};

struct Cond : Node
{
    NP c, t, e;      // e puede ser nullptr
    double eval (Ctx& ctx) const override
    {
        if (truthy (c->eval (ctx))) return t->eval (ctx);
        return e ? e->eval (ctx) : 0.0;
    }
};

struct Seq : Node
{
    std::vector<NP> v;
    double eval (Ctx& c) const override
    {
        double r = 0.0;
        for (auto& n : v) r = n->eval (c);
        return r;
    }
};

struct Assign : Node
{
    int op;      // -1 = '=', si no un Op
    NP lhs, rhs;
    Assign (int o, NP l, NP r) : op (o), lhs (std::move (l)), rhs (std::move (r)) {}
    double eval (Ctx& c) const override
    {
        double v = rhs->eval (c);
        double* p = lhs->slot (c);
        if (op >= 0) v = applyBin (op, *p, v);
        *p = v;
        return v;
    }
};

struct Loop : Node
{
    NP count, body;
    double eval (Ctx& c) const override
    {
        const double cnt = count->eval (c);
        const int64 n = cnt > 1.0e6 ? 1000000 : (cnt > 0.0 ? (int64) cnt : 0);
        for (int64 i = 0; i < n; ++i)
        {
            if (++c.steps > c.maxSteps) { c.aborted = true; break; }
            body->eval (c);
        }
        return 0.0;
    }
};

struct While : Node
{
    NP cond, body;      // body == nullptr: while(código) = repetir mientras el código devuelva distinto de 0
    double eval (Ctx& c) const override
    {
        for (;;)
        {
            if (++c.steps > c.maxSteps) { c.aborted = true; break; }
            if (body)
            {
                if (! truthy (cond->eval (c))) break;
                body->eval (c);
            }
            else if (! truthy (cond->eval (c)))
                break;
        }
        return 0.0;
    }
};

struct FuncInst
{
    NP body;
    int nslots = 0;
    int nparams = 0;
};

struct Call : Node
{
    FuncInst* fi = nullptr;
    std::vector<NP> args;

    double eval (Ctx& c) const override
    {
        const int base = c.sp;
        const int need = fi->nslots;
        if (c.depth >= 200 || base + need > c.stackSize) { c.aborted = true; return 0.0; }

        ++c.depth;
        c.sp = base + need;
        for (int i = 0; i < need; ++i) c.stack[base + i] = 0.0;
        for (size_t i = 0; i < args.size(); ++i) c.stack[base + (int) i] = args[i]->eval (c);

        const int savedFp = c.fp;
        c.fp = base;
        const double r = fi->body ? fi->body->eval (c) : 0.0;
        c.fp = savedFp;
        c.sp = base;
        --c.depth;
        return r;
    }
};

// Funciones matemáticas y utilidades integradas
enum Fn { fSin, fCos, fTan, fAsin, fAcos, fAtan, fAtan2, fSqrt, fPow, fExp, fLog, fLog10, fAbs, fMin, fMax, fSign,
          fFloor, fCeil, fInt, fRand, fSqr, fInvSqrt, fMemset, fMemcpy, fNoop, fTime, fSinh, fCosh, fTanh };

struct FnDef { const char* name; int id; int nargs; };

inline const FnDef* findBuiltin (const String& name)
{
    static const FnDef table[] = {
        { "sin", fSin, 1 }, { "cos", fCos, 1 }, { "tan", fTan, 1 }, { "asin", fAsin, 1 }, { "acos", fAcos, 1 },
        { "atan", fAtan, 1 }, { "atan2", fAtan2, 2 }, { "sqrt", fSqrt, 1 }, { "pow", fPow, 2 }, { "exp", fExp, 1 },
        { "log", fLog, 1 }, { "log10", fLog10, 1 }, { "abs", fAbs, 1 }, { "min", fMin, 2 }, { "max", fMax, 2 },
        { "sign", fSign, 1 }, { "floor", fFloor, 1 }, { "ceil", fCeil, 1 }, { "int", fInt, 1 }, { "rand", fRand, 1 },
        { "sqr", fSqr, 1 }, { "invsqrt", fInvSqrt, 1 }, { "memset", fMemset, 3 }, { "memcpy", fMemcpy, 3 },
        { "freembuf", fNoop, 1 }, { "sliderchange", fNoop, 1 }, { "slider_automate", fNoop, 1 },
        { "slider_show", fNoop, 2 }, { "time", fTime, 0 }, { "time_precise", fTime, 0 },
        { "sinh", fSinh, 1 }, { "cosh", fCosh, 1 }, { "tanh", fTanh, 1 }
    };
    for (auto& f : table)
        if (name == f.name) return &f;
    return nullptr;
}

struct Builtin : Node
{
    int id = 0;
    std::vector<NP> a;

    double eval (Ctx& c) const override
    {
        double x = 0.0, y = 0.0, z = 0.0;
        if (a.size() > 0) x = a[0]->eval (c);
        if (a.size() > 1) y = a[1]->eval (c);
        if (a.size() > 2) z = a[2]->eval (c);

        switch (id)
        {
            case fSin:   return std::sin (x);
            case fCos:   return std::cos (x);
            case fTan:   return std::tan (x);
            case fAsin:  return std::asin (jlimit (-1.0, 1.0, x));
            case fAcos:  return std::acos (jlimit (-1.0, 1.0, x));
            case fAtan:  return std::atan (x);
            case fAtan2: return std::atan2 (x, y);
            case fSqrt:  return std::sqrt (std::abs (x));
            case fPow:   return std::pow (x, y);
            case fExp:   return std::exp (jmin (x, 700.0));
            case fLog:   return x > 0.0 ? std::log (x) : 0.0;
            case fLog10: return x > 0.0 ? std::log10 (x) : 0.0;
            case fAbs:   return std::abs (x);
            case fMin:   return jmin (x, y);
            case fMax:   return jmax (x, y);
            case fSign:  return x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0);
            case fFloor: return std::floor (x);
            case fCeil:  return std::ceil (x);
            case fInt:   return (double) toI64 (x);
            case fSqr:   return x * x;
            case fInvSqrt: return x != 0.0 ? 1.0 / std::sqrt (std::abs (x)) : 0.0;
            case fSinh:  return std::sinh (jlimit (-700.0, 700.0, x));
            case fCosh:  return std::cosh (jlimit (-700.0, 700.0, x));
            case fTanh:  return std::tanh (x);
            case fRand:
            {
                c.rng = c.rng * 1664525u + 1013904223u;
                return x * ((double) (c.rng >> 8) / 16777216.0);
            }
            case fTime:  return Time::getMillisecondCounterHiRes() / 1000.0;
            case fMemset:
            {
                const int64 d = toI64 (x), n = toI64 (z);
                for (int64 i = 0; i < n && d + i < c.memSize; ++i)
                    if (d + i >= 0) c.mem[d + i] = y;
                return x;
            }
            case fMemcpy:
            {
                int64 d = toI64 (x), s = toI64 (y), n = toI64 (z);
                if (d < 0) { n += d; s -= d; d = 0; }
                if (s < 0) { n += s; d -= s; s = 0; }
                n = jmin (n, (int64) c.memSize - d, (int64) c.memSize - s);
                if (n > 0) std::memmove (c.mem + d, c.mem + s, (size_t) n * sizeof (double));
                return x;
            }
            default: return 0.0;
        }
    }
};

// ---------------------------------------------------------------------------
// Símbolos y compilador
// ---------------------------------------------------------------------------
struct FuncDef
{
    String name;
    StringArray params, locals, instances;
    const std::vector<Token>* toks = nullptr;
    size_t bodyBegin = 0, bodyEnd = 0;
    int line = 0;
};

struct Symbols
{
    std::map<String, int> varIndex;
    std::map<String, FuncDef> funcs;
    std::map<String, std::unique_ptr<FuncInst>> insts;
    int splBase = 0, sliderBase = 64;

    int varFor (const String& name)
    {
        auto it = varIndex.find (name);
        if (it != varIndex.end()) return it->second;
        const int idx = (int) varIndex.size();
        varIndex[name] = idx;
        return idx;
    }
};

struct Scope
{
    std::map<String, int> locals;
    StringArray instances;
    String prefix;
};

// Busca las definiciones "function nombre(a, b) local(x) instance(y) ( ... )" (primer paso)
inline void scanFunctions (Symbols& sym, const std::vector<Token>& tk, std::map<size_t, size_t>& skip)
{
    int depth = 0;
    for (size_t i = 0; i < tk.size(); ++i)
    {
        const Token& t = tk[i];
        if (t.type == tkOp && t.text == "(") ++depth;
        else if (t.type == tkOp && t.text == ")") --depth;
        else if (depth == 0 && t.type == tkId && t.text == "function")
        {
            FuncDef d;
            d.toks = &tk;
            d.line = t.line;
            size_t j = i + 1;
            if (j >= tk.size() || tk[j].type != tkId) throw Error { t.line, tr ("falta el nombre de la función") };
            d.name = tk[j].text;
            ++j;

            auto readNames = [&] (StringArray& out)
            {
                if (j >= tk.size() || tk[j].type != tkOp || tk[j].text != "(")
                    throw Error { t.line, tr ("falta '(' en la declaración de la función ") + d.name };
                ++j;
                while (j < tk.size() && ! (tk[j].type == tkOp && tk[j].text == ")"))
                {
                    if (tk[j].type == tkId) out.add (tk[j].text);
                    else if (! (tk[j].type == tkOp && tk[j].text == ","))
                        throw Error { tk[j].line, tr ("nombre inválido en la declaración de la función ") + d.name };
                    ++j;
                }
                if (j >= tk.size()) throw Error { t.line, tr ("falta ')' en la declaración de la función ") + d.name };
                ++j;
            };

            readNames (d.params);
            while (j < tk.size() && tk[j].type == tkId
                   && (tk[j].text == "local" || tk[j].text == "instance" || tk[j].text == "global" || tk[j].text == "globals"))
            {
                const String kw = tk[j].text;
                ++j;
                StringArray names;
                readNames (names);
                if (kw == "local") d.locals.addArray (names);
                else if (kw == "instance") d.instances.addArray (names);
            }

            if (j >= tk.size() || tk[j].type != tkOp || tk[j].text != "(")
                throw Error { t.line, tr ("falta el cuerpo '( ... )' de la función ") + d.name };

            int dd = 0;
            size_t k = j;
            for (; k < tk.size(); ++k)
            {
                if (tk[k].type == tkOp && tk[k].text == "(") ++dd;
                else if (tk[k].type == tkOp && tk[k].text == ")") { if (--dd == 0) break; }
            }
            if (k >= tk.size()) throw Error { t.line, tr ("falta cerrar el cuerpo de la función ") + d.name };

            d.bodyBegin = j + 1;
            d.bodyEnd = k;
            sym.funcs[d.name] = d;
            skip[i] = k + 1;
            i = k;
        }
    }
}

class Compiler
{
public:
    Compiler (Symbols& s, const std::vector<Token>& t, size_t begin, size_t end, Scope* sc,
              const std::map<size_t, size_t>* skip)
        : sym (s), toks (t), pos (begin), endPos (jmin (end, t.size())), scope (sc), skipMap (skip) {}

    // closer: "" = hasta el final; ")" = hasta el paréntesis que cierra
    NP parseSeq (const char* closer, bool stopAtComma = false)
    {
        auto seq = std::make_unique<Seq>();
        const bool hasCloser = closer[0] != 0;

        for (;;)
        {
            while (isOp (";")) ++pos;

            if (skipMap != nullptr)
            {
                auto it = skipMap->find (pos);
                if (it != skipMap->end()) { pos = it->second; continue; }
            }

            if (atEnd())
            {
                if (hasCloser) fail (tr ("falta cerrar un paréntesis ')'"));
                break;
            }
            if (hasCloser && isOp (closer)) break;
            if (stopAtComma && isOp (",")) break;

            seq->v.push_back (parseAssign());

            if (isOp (";") || atEnd()) continue;
            if (hasCloser && isOp (closer)) continue;
            if (stopAtComma && isOp (",")) continue;
            fail (tr ("se esperaba ';' cerca de '") + cur().text + "'");
        }

        if (seq->v.empty()) return std::make_unique<Num> (0.0);
        if (seq->v.size() == 1) return std::move (seq->v[0]);
        return seq;
    }

private:
    Symbols& sym;
    const std::vector<Token>& toks;
    size_t pos, endPos;
    Scope* scope;
    const std::map<size_t, size_t>* skipMap;
    Token endTok;

    bool atEnd() const { return pos >= endPos; }

    const Token& cur()
    {
        if (pos < endPos) return toks[pos];
        endTok.type = tkEnd;
        endTok.text = tr ("<fin>");
        endTok.line = toks.empty() ? 0 : toks[jmin (toks.size() - 1, endPos > 0 ? endPos - 1 : (size_t) 0)].line;
        return endTok;
    }

    bool isOp (const char* o)
    {
        const Token& t = cur();
        return t.type == tkOp && t.text == o;
    }

    [[noreturn]] void fail (const String& msg)
    {
        throw Error { cur().line, msg };
    }

    void expect (const char* o)
    {
        if (! isOp (o)) fail (tr ("se esperaba '") + String (o) + tr ("' cerca de '") + cur().text + "'");
        ++pos;
    }

    NP globalVar (const String& name) { return std::make_unique<GVar> (sym.varFor (name)); }

    NP makeVar (const String& name)
    {
        if (name == "else" || name == "function")
            fail (tr ("'") + name + tr ("' en un lugar inesperado"));

        if (scope != nullptr)
        {
            auto it = scope->locals.find (name);
            if (it != scope->locals.end()) return std::make_unique<LVar> (it->second);
            if (name.startsWith ("this.")) return globalVar (scope->prefix + name.substring (5));
            if (scope->instances.contains (name)) return globalVar (scope->prefix + name);
        }
        return globalVar (name);
    }

    NP parseAssign()
    {
        NP lhs = parseTernary();
        if (cur().type == tkOp)
        {
            struct A { const char* t; int code; };
            static const A table[] = { { "=", -1 }, { "+=", opAdd }, { "-=", opSub }, { "*=", opMul }, { "/=", opDiv },
                                       { "%=", opMod }, { "|=", opOr }, { "&=", opAnd }, { "~=", opXor }, { "^=", opPow } };
            for (auto& a : table)
                if (cur().text == a.t)
                {
                    if (! lhs->isLvalue()) fail (tr ("a la izquierda de '") + String (a.t) + tr ("' debe haber una variable"));
                    ++pos;
                    NP rhs = parseAssign();
                    return std::make_unique<Assign> (a.code, std::move (lhs), std::move (rhs));
                }
        }
        return lhs;
    }

    NP parseTernary()
    {
        NP c = parseOr();
        if (! isOp ("?")) return c;
        ++pos;

        auto n = std::make_unique<Cond>();
        n->c = std::move (c);
        n->t = parseAssign();
        if (isOp (":"))
        {
            ++pos;
            n->e = parseAssign();
        }
        return n;
    }

    NP parseOr()
    {
        NP l = parseAnd();
        while (isOp ("||"))
        {
            ++pos;
            NP r = parseAnd();
            l = std::make_unique<OrN> (std::move (l), std::move (r));
        }
        return l;
    }

    NP parseAnd()
    {
        NP l = parseLevel (0);
        while (isOp ("&&"))
        {
            ++pos;
            NP r = parseLevel (0);
            l = std::make_unique<AndN> (std::move (l), std::move (r));
        }
        return l;
    }

    struct OpEntry { const char* t; int code; };

    // 0 comparaciones, 1 | & ~, 2 << >>, 3 + -, 4 * /, 5 %
    NP parseLevel (int lv)
    {
        if (lv > 5) return parseUnary();

        static const std::vector<std::vector<OpEntry>> levels = {
            { { "==", opEq }, { "===", opEqX }, { "!=", opNe }, { "!==", opNeX }, { "<", opLt }, { ">", opGt }, { "<=", opLe }, { ">=", opGe } },
            { { "|", opOr }, { "&", opAnd }, { "~", opXor } },
            { { "<<", opShl }, { ">>", opShr } },
            { { "+", opAdd }, { "-", opSub } },
            { { "*", opMul }, { "/", opDiv } },
            { { "%", opMod } }
        };

        NP l = parseLevel (lv + 1);
        for (;;)
        {
            int code = -100;
            if (cur().type == tkOp)
                for (auto& e : levels[(size_t) lv])
                    if (cur().text == e.t) { code = e.code; break; }

            if (code == -100) return l;
            ++pos;
            NP r = parseLevel (lv + 1);
            l = std::make_unique<Bin> (code, std::move (l), std::move (r));
        }
    }

    NP parseUnary()
    {
        if (isOp ("-")) { ++pos; NP a = parseUnary(); return std::make_unique<Un> (0, std::move (a)); }
        if (isOp ("+")) { ++pos; return parseUnary(); }
        if (isOp ("!")) { ++pos; NP a = parseUnary(); return std::make_unique<Un> (1, std::move (a)); }
        return parsePow();
    }

    NP parsePow()
    {
        NP base = parsePrimary();
        if (isOp ("^"))
        {
            ++pos;
            NP e = parseUnary();
            return std::make_unique<Bin> (opPow, std::move (base), std::move (e));
        }
        return base;
    }

    NP parseGroup()
    {
        expect ("(");
        NP n = parseSeq (")");
        expect (")");
        return n;
    }

    NP parsePrimary()
    {
        const Token t = cur();
        if (t.type == tkNum) { ++pos; return std::make_unique<Num> (t.num); }
        if (t.type == tkOp && t.text == "(") return parseGroup();
        if (t.type == tkId) return parseIdent();
        fail (tr ("expresión inesperada cerca de '") + t.text + "'");
    }

    NP parseIdent()
    {
        const Token t = cur();
        ++pos;
        const String name = t.text;

        if (isOp ("("))
        {
            if (name == "if")    return parseIf();
            if (name == "while") return parseWhile();
            if (name == "loop")  return parseLoop();
            return parseCall (name);
        }

        if (isOp ("["))
        {
            ++pos;
            NP idx;
            if (isOp ("]")) idx = std::make_unique<Num> (0.0);
            else idx = parseAssign();
            expect ("]");

            auto m = std::make_unique<MemIdx>();
            if (name == "mem" || name == "gmem")
            {
                m->base = std::make_unique<Num> (0.0);
                m->global = (name == "gmem");
            }
            else
                m->base = makeVar (name);
            m->idx = std::move (idx);
            return m;
        }

        return makeVar (name);
    }

    NP parseIf()
    {
        expect ("(");
        NP cond = parseSeq (")", true);

        auto n = std::make_unique<Cond>();
        n->c = std::move (cond);

        if (isOp (","))            // if (cond, entonces, si no)
        {
            ++pos;
            n->t = parseAssign();
            if (isOp (",")) { ++pos; n->e = parseAssign(); }
            expect (")");
            return n;
        }

        expect (")");
        if (! isOp ("(")) fail (tr ("después de if (condición) debe ir un bloque entre paréntesis ( ... )"));
        n->t = parseGroup();

        if (cur().type == tkId && cur().text == "else")
        {
            ++pos;
            if (cur().type == tkId && cur().text == "if") n->e = parsePrimary();
            else
            {
                if (! isOp ("(")) fail (tr ("después de else debe ir un bloque entre paréntesis ( ... )"));
                n->e = parseGroup();
            }
        }
        return n;
    }

    NP parseWhile()
    {
        expect ("(");
        NP code = parseSeq (")");
        expect (")");

        auto n = std::make_unique<While>();
        n->cond = std::move (code);
        if (isOp ("(")) n->body = parseGroup();      // while (condición) ( cuerpo )
        return n;
    }

    NP parseLoop()
    {
        expect ("(");
        auto n = std::make_unique<Loop>();
        n->count = parseAssign();
        expect (",");
        n->body = parseSeq (")");
        expect (")");
        return n;
    }

    NP parseCall (const String& name)
    {
        const int line = cur().line;
        expect ("(");
        std::vector<NP> args;
        if (! isOp (")"))
            for (;;)
            {
                args.push_back (parseAssign());
                if (isOp (",")) { ++pos; continue; }
                break;
            }
        expect (")");

        if (name == "spl" || name == "slider")
        {
            if (args.size() != 1) throw Error { line, tr ("la función ") + name + tr (" espera 1 argumento") };
            auto n = std::make_unique<IdxVar>();
            n->idx = std::move (args[0]);
            if (name == "spl") { n->base = sym.splBase; n->count = 64; n->offset = 0; }
            else               { n->base = sym.sliderBase; n->count = 64; n->offset = 1; }
            return n;
        }

        if (const FnDef* f = findBuiltin (name))
        {
            if ((int) args.size() != f->nargs)
                throw Error { line, tr ("la función ") + name + tr (" espera ") + String (f->nargs) + tr (" argumento(s)") };
            auto n = std::make_unique<Builtin>();
            n->id = f->id;
            n->a = std::move (args);
            return n;
        }

        // función propia (con o sin namespace: obj.fn(...) / this.fn(...))
        String fn = name, prefix;
        const int dot = name.lastIndexOfChar ('.');
        if (dot >= 0)
        {
            fn = name.substring (dot + 1);
            const String ns = name.substring (0, dot);
            const String base = scope != nullptr ? scope->prefix : String();
            if (ns == "this") prefix = base;
            else if (ns.startsWith ("this.")) prefix = base + ns.substring (5) + ".";
            else prefix = ns + ".";
        }

        auto it = sym.funcs.find (fn);
        if (it == sym.funcs.end())
            throw Error { line, tr ("función no definida o no soportada: ") + name + "()" };
        if ((int) args.size() != it->second.params.size())
            throw Error { line, tr ("la función ") + fn + tr (" espera ") + String (it->second.params.size()) + tr (" argumento(s)") };

        auto n = std::make_unique<Call>();
        n->fi = getInst (it->second, prefix);
        n->args = std::move (args);
        return n;
    }

    FuncInst* getInst (const FuncDef& def, const String& prefix)
    {
        const String key = def.name + "|" + prefix;
        auto it = sym.insts.find (key);
        if (it != sym.insts.end()) return it->second.get();

        auto fi = std::make_unique<FuncInst>();
        FuncInst* raw = fi.get();
        sym.insts[key] = std::move (fi);        // se registra antes de compilar (permite recursión)

        Scope sc;
        sc.prefix = prefix;
        sc.instances = def.instances;
        int slot = 0;
        for (auto& p : def.params) sc.locals[p] = slot++;
        for (auto& l : def.locals) sc.locals[l] = slot++;
        raw->nparams = def.params.size();
        raw->nslots = slot;

        Compiler sub (sym, *def.toks, def.bodyBegin, def.bodyEnd, &sc, nullptr);
        raw->body = sub.parseSeq ("");
        return raw;
    }
};

// ---------------------------------------------------------------------------
// Programa compilado + estado de ejecución
// ---------------------------------------------------------------------------
struct SectionSrc
{
    String text;
    int firstLine = 1;
    bool present = false;
};

class Program
{
public:
    static constexpr int memSize = 1 << 20;      // 1M posiciones de mem[] por instancia (8 MB)
    static constexpr int stackSize = 1 << 14;

    Symbols sym;
    Ctx ctx;
    std::vector<double> vars, mem, stack;
    NP secInit, secSlider, secBlock, secSample;
    int vSrate = 0, vSamplesBlock = 0, vNumCh = 0, vTempo = 0, vPlayState = 0, vPlayPos = 0, vBeatPos = 0,
        vTsNum = 0, vTsDenom = 0;

    Program()
    {
        for (int i = 0; i < 64; ++i) sym.varFor ("spl" + String (i));
        sym.splBase = sym.varIndex["spl0"];
        for (int i = 1; i <= 64; ++i) sym.varFor ("slider" + String (i));
        sym.sliderBase = sym.varIndex["slider1"];
        vSrate = sym.varFor ("srate");
        vSamplesBlock = sym.varFor ("samplesblock");
        vNumCh = sym.varFor ("num_ch");
        vTempo = sym.varFor ("tempo");
        vPlayState = sym.varFor ("play_state");
        vPlayPos = sym.varFor ("play_position");
        vBeatPos = sym.varFor ("beat_position");
        vTsNum = sym.varFor ("ts_num");
        vTsDenom = sym.varFor ("ts_denom");
        for (auto* n : { "pdc_delay", "pdc_bot_ch", "pdc_top_ch", "ext_noinit", "ext_nodenorm", "ext_midi_bus", "trigger", "midi_bus" })
            sym.varFor (n);
    }

    // Traduce las secciones a árboles ejecutables. Lanza jsfx::Error (con línea) si hay un fallo.
    void compile (const SectionSrc (&src)[4])
    {
        std::map<size_t, size_t> skip[4];
        for (int s = 0; s < 4; ++s)
            if (src[s].present)
            {
                tokenize (src[s].text, src[s].firstLine, tokens[s]);
                scanFunctions (sym, tokens[s], skip[s]);
            }

        NP* dst[4] = { &secInit, &secSlider, &secBlock, &secSample };
        for (int s = 0; s < 4; ++s)
            if (src[s].present)
            {
                Compiler c (sym, tokens[s], 0, tokens[s].size(), nullptr, &skip[s]);
                *dst[s] = c.parseSeq ("");
            }
    }

    // Reserva y pone a cero la memoria (al cargar y cada vez que cambia la frecuencia de muestreo)
    void prepare (double sampleRate, int blockSize)
    {
        vars.assign (sym.varIndex.size() + 1, 0.0);
        mem.assign ((size_t) memSize, 0.0);
        stack.assign ((size_t) stackSize, 0.0);

        auto& g = sharedGmem();
        ctx.vars = vars.data();
        ctx.mem = mem.data();         ctx.memSize = memSize;
        ctx.gmem = g.data();          ctx.gmemSize = (int) g.size();
        ctx.stack = stack.data();     ctx.stackSize = stackSize;
        ctx.sp = ctx.fp = ctx.depth = 0;
        ctx.aborted = false;
        ctx.steps = 0;

        vars[(size_t) vSrate] = sampleRate;
        vars[(size_t) vSamplesBlock] = blockSize;
        vars[(size_t) vNumCh] = 2.0;
        vars[(size_t) vTempo] = 120.0;
        vars[(size_t) vPlayState] = 1.0;
        vars[(size_t) vTsNum] = 4.0;
        vars[(size_t) vTsDenom] = 4.0;
    }

    void run (const NP& n)
    {
        if (n == nullptr || ctx.aborted) return;
        ctx.sp = 0;
        ctx.fp = 0;
        ctx.depth = 0;
        n->eval (ctx);
    }

private:
    std::vector<Token> tokens[4];

    static std::vector<double>& sharedGmem()
    {
        static std::vector<double> g (65536, 0.0);
        return g;
    }
};

// ---------------------------------------------------------------------------
// Análisis de la cabecera del archivo .jsfx
// ---------------------------------------------------------------------------
inline bool parseSliderLine (const String& line, SliderInfo& s)
{
    const int colon = line.indexOfChar (':');
    if (colon <= 6) return false;
    const String numStr = line.substring (6, colon);
    if (! numStr.containsOnly ("0123456789")) return false;

    s.index = numStr.getIntValue();
    if (s.index < 1 || s.index > 64) return false;

    String rest = line.substring (colon + 1).trim();

    if (rest.startsWithChar ('/'))        // slider de archivo (/carpeta:defecto:nombre): no es un parámetro numérico
    {
        s.hidden = true;
        s.name = rest;
        return true;
    }

    const int eq = rest.indexOfChar ('=');
    const int lt0 = rest.indexOfChar ('<');
    if (eq > 0 && (lt0 < 0 || eq < lt0))      // sliderN:variable=valor<...>nombre
    {
        const String v = rest.substring (0, eq).trim().toLowerCase();
        if (v.isNotEmpty() && ! v.containsAnyOf (" \t,<>{}:"))
        {
            s.varName = v;
            rest = rest.substring (eq + 1);
        }
    }

    String defStr, rangeStr, name;
    const int lt = rest.indexOfChar ('<');
    if (lt >= 0)
    {
        defStr = rest.substring (0, lt);
        int depth = 0, gt = -1;
        for (int j = lt + 1; j < rest.length(); ++j)
        {
            const juce_wchar ch = rest[j];
            if (ch == '{') ++depth;
            else if (ch == '}') --depth;
            else if (ch == '>' && depth <= 0) { gt = j; break; }
        }
        if (gt < 0) return false;
        rangeStr = rest.substring (lt + 1, gt);
        name = rest.substring (gt + 1);
    }
    else
    {
        const int sp = rest.indexOfAnyOf (" \t,:");
        defStr = sp < 0 ? rest : rest.substring (0, sp);
        name = sp < 0 ? String() : rest.substring (sp + 1);
    }

    s.defV = defStr.trim().getDoubleValue();

    const int br = rangeStr.indexOfChar ('{');
    String nums = br >= 0 ? rangeStr.substring (0, br) : rangeStr;
    if (br >= 0)
    {
        const int cb = rangeStr.lastIndexOfChar ('}');
        s.enumNames.addTokens (rangeStr.substring (br + 1, cb < 0 ? rangeStr.length() : cb), ",", "");
        s.enumNames.trim();
    }
    const int cc = nums.indexOfChar (':');           // modificadores como :log o :sqr=2
    if (cc >= 0) nums = nums.substring (0, cc);

    StringArray p;
    p.addTokens (nums, ",", "");
    if (p.size() >= 1 && p[0].trim().isNotEmpty()) s.minV = p[0].getDoubleValue();
    if (p.size() >= 2 && p[1].trim().isNotEmpty()) s.maxV = p[1].getDoubleValue();
    if (p.size() >= 3 && p[2].trim().isNotEmpty()) s.step = p[2].getDoubleValue();

    if (! s.enumNames.isEmpty() && p.size() < 2)
    {
        s.minV = 0.0;
        s.maxV = (double) (s.enumNames.size() - 1);
        s.step = 1.0;
    }
    if (! s.enumNames.isEmpty() && s.step <= 0.0) s.step = 1.0;
    if (s.maxV <= s.minV) s.maxV = s.minV + 1.0;
    s.defV = jlimit (s.minV, s.maxV, s.defV);

    name = name.trim();
    if (name.startsWithChar ('-')) { s.hidden = true; name = name.substring (1).trim(); }
    s.name = name.isEmpty() ? "slider" + String (s.index) : name;
    return true;
}

// Lee un .jsfx: devuelve false y un mensaje (con el número de línea) si no se puede cargar.
inline bool compileFile (const File& f, Info& info, std::unique_ptr<Program>& prog, String& err)
{
    if (! f.existsAsFile()) { err = tr ("El archivo no existe:\n") + f.getFullPathName(); return false; }

    const String text = f.loadFileAsString();
    if (text.isEmpty()) { err = tr ("No se pudo leer el archivo (está vacío o sin permiso)."); return false; }

    StringArray lines;
    lines.addLines (text);

    info = Info();
    info.file = f.getFullPathName();
    info.name = f.getFileNameWithoutExtension();
    prog = std::make_unique<Program>();

    SectionSrc src[4];
    int cur = -1;          // -1 cabecera, 0..3 secciones que se compilan, 4 secciones ignoradas

    try
    {
        for (int i = 0; i < lines.size(); ++i)
        {
            const String raw = lines[i];
            const String line = raw.trim();

            if (line.startsWithChar ('@'))
            {
                const String word = line.substring (1).upToFirstOccurrenceOf (" ", false, false).trim().toLowerCase();
                if (word == "init") cur = 0;
                else if (word == "slider") cur = 1;
                else if (word == "block") cur = 2;
                else if (word == "sample") cur = 3;
                else
                {
                    cur = 4;
                    if (word == "gfx") info.warnings.addIfNotAlreadyThere (tr ("@gfx (interfaz gráfica propia) no se muestra: usa los sliders"));
                    else if (word == "serialize") info.warnings.addIfNotAlreadyThere (tr ("@serialize se ignora"));
                    else if (word == "import") info.warnings.addIfNotAlreadyThere (tr ("@import se ignora"));
                    else info.warnings.addIfNotAlreadyThere (tr ("sección @") + word + tr (" no soportada (se ignora)"));
                }

                if (cur < 4)
                {
                    if (src[cur].present) throw Error { i + 1, tr ("la sección @") + word + tr (" aparece dos veces") };
                    src[cur].present = true;
                    src[cur].firstLine = i + 2;      // la primera línea de código es la siguiente (1-based)
                }
                continue;
            }

            if (cur == -1)
            {
                if (line.isEmpty() || line.startsWith ("//")) continue;

                if (line.startsWith ("desc:"))
                {
                    info.desc = line.substring (5).trim();
                    if (info.desc.isNotEmpty()) info.name = info.desc;
                }
                else if (line.startsWith ("tags:"))    info.tags = line.substring (5).trim();
                else if (line.startsWith ("in_pin:"))
                {
                    info.pinsDeclared = true;
                    const String n = line.substring (7).trim();
                    if (n.compareIgnoreCase ("none") != 0) info.inPins.add (n.isEmpty() ? "In" : n);
                }
                else if (line.startsWith ("out_pin:"))
                {
                    info.pinsDeclared = true;
                    const String n = line.substring (8).trim();
                    if (n.compareIgnoreCase ("none") != 0) info.outPins.add (n.isEmpty() ? "Out" : n);
                }
                else if (line.startsWithIgnoreCase ("slider") && CharacterFunctions::isDigit (line[6]))
                {
                    SliderInfo s;
                    if (! parseSliderLine (line, s))
                        throw Error { i + 1, tr ("línea de slider mal escrita (formato: sliderN:defecto<min,max,paso>nombre)") };
                    for (auto& o : info.sliders)
                        if (o.index == s.index) throw Error { i + 1, tr ("slider") + String (s.index) + tr (" está repetido") };
                    info.sliders.push_back (s);
                }
                else if (line.startsWith ("import"))   info.warnings.addIfNotAlreadyThere (tr ("import de otras librerías no soportado: ") + line);
                else if (line.startsWith ("filename:")) info.warnings.addIfNotAlreadyThere (tr ("filename: se ignora (archivos de audio de datos no soportados)"));
                continue;
            }

            if (cur >= 0 && cur < 4)
                src[cur].text << raw << "\n";
        }

        if (info.pinsDeclared && (info.inPins.size() > 8 || info.outPins.size() > 8))
            info.warnings.add (tr ("el DAW procesa como máximo 8 canales"));

        // variables propias de los sliders (sliderN:miVar=...) antes de compilar, para que el código las encuentre
        for (auto& s : info.sliders)
            if (s.varName.isNotEmpty()) s.varIdx = prog->sym.varFor (s.varName);

        prog->compile (src);
        return true;
    }
    catch (const Error& e)
    {
        String where;
        where << tr ("Error en la línea ") << String (e.line) << tr (" de ") << f.getFileName() << ":\n" << e.msg;
        if (e.line >= 1 && e.line <= lines.size())
            where << "\n\n    " << String (e.line) << " | " << lines[e.line - 1].trim();
        err = where;
        prog.reset();
        return false;
    }
}
} // namespace jsfx

// ===========================================================================
// El plugin: un JSFX dentro de la cadena de FX
// ===========================================================================
class JsfxProcessor : public AudioPluginInstance
{
public:
    // Devuelve nullptr y un mensaje con la línea del error si el JSFX no se puede cargar.
    static std::unique_ptr<JsfxProcessor> create (const File& f, double sr, int bs, String& err)
    {
        jsfx::Info info;
        std::unique_ptr<jsfx::Program> prog;
        if (! jsfx::compileFile (f, info, prog, err))
            return nullptr;

        std::unique_ptr<JsfxProcessor> p (new JsfxProcessor (std::move (info), std::move (prog)));
        p->setRateAndBufferSizeDetails (sr, bs);
        return p;
    }

    const jsfx::Info& getInfo() const { return info; }

    // --- AudioProcessor ---
    const String getName() const override { return info.name; }

    void fillInPluginDescription (PluginDescription& d) const override
    {
        d.name = info.name;
        d.descriptiveName = info.desc.isNotEmpty() ? info.desc : info.name;
        d.pluginFormatName = "JSFX";
        d.category = "JSFX";
        d.manufacturerName = "JSFX";
        d.version = "1.0";
        d.fileOrIdentifier = info.file;
        d.lastFileModTime = File (info.file).getLastModificationTime();
        d.lastInfoUpdateTime = Time::getCurrentTime();
        d.uniqueId = d.deprecatedUid = (int) info.file.hashCode();
        d.isInstrument = false;
        d.numInputChannels = 2;
        d.numOutputChannels = 2;
    }

    void prepareToPlay (double sr, int bs) override
    {
        ready = false;
        failed = false;
        prog->prepare (sr, jmax (1, bs));

        double* v = prog->vars.data();
        for (auto& s : info.sliders)             // valores por defecto (también de los sliders ocultos)
        {
            v[prog->sym.sliderBase + s.index - 1] = s.defV;
            if (s.varIdx >= 0) v[s.varIdx] = s.defV;
        }
        for (auto& q : pars)                     // y los valores actuales de los parámetros
        {
            const double val = valueOf (q);
            q.last = val;
            v[prog->sym.sliderBase + q.slider - 1] = val;
            if (q.var >= 0) v[q.var] = val;
        }

        prog->ctx.steps = 0;
        prog->ctx.maxSteps = 50000000;
        prog->run (prog->secInit);
        prog->run (prog->secSlider);
        failed = prog->ctx.aborted;
        ready = true;
    }

    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& buf, MidiBuffer&) override
    {
        ScopedNoDenormals noDenormals;
        if (! ready || failed) return;                   // si el código se atascó, el efecto queda en paso directo

        const int n = buf.getNumSamples();
        const int bufCh = buf.getNumChannels();
        if (n <= 0 || bufCh <= 0) return;

        double* v = prog->vars.data();
        auto& ctx = prog->ctx;
        ctx.steps = 0;
        ctx.maxSteps = 4000000;          // tope de iteraciones de bucles por bloque (evita que un bucle infinito congele el DAW)

        // sliders que cambiaron (mouse o automatización) -> @slider
        bool changed = false;
        for (auto& q : pars)
        {
            const double val = valueOf (q);
            if (val != q.last)
            {
                q.last = val;
                v[prog->sym.sliderBase + q.slider - 1] = val;
                if (q.var >= 0) v[q.var] = val;
                changed = true;
            }
        }
        if (changed) prog->run (prog->secSlider);

        const int nIn = info.numIn(), nOut = info.numOut();
        const bool monoIn = info.pinsDeclared && nIn == 1 && bufCh >= 2;      // entrada mono: se promedian L y R
        int nch = info.pinsDeclared ? jmin (bufCh, jmax (1, jmax (nIn, nOut))) : jmin (bufCh, 8);
        if (monoIn) nch = 2;
        nch = jmin (nch, 8);

        float* ch[8] = {};
        for (int c = 0; c < nch; ++c) ch[c] = buf.getWritePointer (c);

        v[prog->vSamplesBlock] = n;
        v[prog->vNumCh] = nch;
        prog->run (prog->secBlock);

        if (prog->secSample != nullptr && ! ctx.aborted)
        {
            const int spl = prog->sym.splBase;
            const jsfx::Node* sampleNode = prog->secSample.get();

            for (int i = 0; i < n; ++i)
            {
                if (monoIn) v[spl] = 0.5 * ((double) ch[0][i] + (double) ch[1][i]);
                else        for (int c = 0; c < nch; ++c) v[spl + c] = (double) ch[c][i];

                ctx.sp = 0; ctx.fp = 0; ctx.depth = 0;
                sampleNode->eval (ctx);

                if (monoIn)
                {
                    const float o0 = clean (v[spl]);
                    ch[0][i] = o0;
                    ch[1][i] = nOut >= 2 ? clean (v[spl + 1]) : o0;
                }
                else
                    for (int c = 0; c < nch; ++c) ch[c][i] = clean (v[spl + c]);
            }
        }

        if (ctx.aborted) failed = true;
    }

    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& dest) override
    {
        XmlElement xml ("JSFXSTATE");
        for (auto& q : pars)
        {
            auto* e = xml.createNewChildElement ("S");
            e->setAttribute ("i", q.slider);
            e->setAttribute ("v", valueOf (q));
        }
        copyXmlToBinary (xml, dest);
    }

    void setStateInformation (const void* data, int size) override
    {
        auto xml = getXmlFromBinary (data, size);
        if (xml == nullptr) return;

        for (auto* e : xml->getChildWithTagNameIterator ("S"))
            for (auto& q : pars)
                if (q.slider == e->getIntAttribute ("i"))
                {
                    const double val = e->getDoubleAttribute ("v");
                    if (q.f != nullptr)
                        q.f->setValueNotifyingHost (q.f->convertTo0to1 ((float) val));
                    else if (q.c != nullptr)
                        q.c->setValueNotifyingHost (q.c->convertTo0to1 ((float) std::round ((val - q.minV) / q.step)));
                }
    }

private:
    struct Par
    {
        int slider = 0;
        AudioParameterFloat* f = nullptr;
        AudioParameterChoice* c = nullptr;
        double minV = 0.0, step = 1.0;
        int var = -1;                 // variable propia del slider (sliderN:var=...)
        double last = -1.0e300;       // último valor entregado al código
    };

    JsfxProcessor (jsfx::Info i, std::unique_ptr<jsfx::Program> p)
        : AudioPluginInstance (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                                .withOutput ("Output", AudioChannelSet::stereo(), true)),
          info (std::move (i)), prog (std::move (p))
    {
        for (auto& s : info.sliders)
        {
            if (s.hidden) continue;

            Par q;
            q.slider = s.index;
            q.minV = s.minV;
            q.var = s.varIdx;
            const String id = "slider" + String (s.index);

            if (! s.enumNames.isEmpty())
            {
                q.step = s.step > 0.0 ? s.step : 1.0;
                const int count = jmax (1, (int) std::round ((s.maxV - s.minV) / q.step) + 1);
                StringArray names = s.enumNames;
                while (names.size() < count) names.add (String (names.size()));
                if (names.size() > count) names.removeRange (count, names.size() - count);
                const int def = jlimit (0, count - 1, (int) std::round ((s.defV - s.minV) / q.step));
                q.c = new AudioParameterChoice (ParameterID (id, 1), s.name, names, def);
                AudioProcessor::addParameter (q.c);
            }
            else
            {
                NormalisableRange<float> range ((float) s.minV, (float) s.maxV, s.step > 0.0 ? (float) s.step : 0.0f);
                q.f = new AudioParameterFloat (ParameterID (id, 1), s.name, range, (float) s.defV);
                AudioProcessor::addParameter (q.f);
            }
            pars.push_back (q);
        }
    }

    double valueOf (const Par& q) const
    {
        if (q.f != nullptr) return (double) q.f->get();
        return q.minV + q.step * (double) q.c->getIndex();
    }

    static float clean (double v)
    {
        const float f = (float) v;
        return std::isfinite (f) ? f : 0.0f;
    }

    jsfx::Info info;
    std::unique_ptr<jsfx::Program> prog;
    std::vector<Par> pars;
    bool ready = false, failed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JsfxProcessor)
};

// Editor: el análisis del efecto (nombre, descripción, entradas/salidas, sliders) arriba y los sliders abajo
class JsfxEditor : public AudioProcessorEditor
{
public:
    JsfxEditor (JsfxProcessor& p, const jsfx::Info& info) : AudioProcessorEditor (p), generic (p)
    {
        const String text = info.report();
        report.setMultiLine (true);
        report.setReadOnly (true);
        report.setCaretVisible (false);
        report.setScrollbarsShown (true);
        report.setFont (Font (FontOptions (12.0f)));
        report.setText (text, false);
        addAndMakeVisible (report);
        addAndMakeVisible (generic);

        const int lines = text.retainCharacters ("\n").length() + 1;
        reportH = jlimit (110, 240, lines * 16 + 14);
        setSize (460, reportH + jmax (80, generic.getHeight()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        report.setBounds (r.removeFromTop (reportH));
        generic.setBounds (r);
    }

private:
    TextEditor report;
    GenericAudioProcessorEditor generic;
    int reportH = 120;
};

inline AudioProcessorEditor* JsfxProcessor::createEditor() { return new JsfxEditor (*this, info); }
