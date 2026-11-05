/* object_code.c - Generador de código objeto: TAC -> assembly x86-64 (AT&T).
   Cada variable local, parámetro y temporal tiene un slot de
   8 bytes en la pila. Cada instrucción TAC carga de la pila a %r10/%r11,
   opera y guarda el resultado de vuelta en la pila. Los int son de 32 bits:
   en el slot se guardan extendidos en signo a 64 (movslq).
   Con -opt tempslots, los temporales que no están vivos al mismo tiempo comparten slot. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object_code.h"
#include "symbol_table.h"

#define MAX_SLOTS 4096

static FILE *out;

/* Slots de la función actual: primero los parámetros y variables (f_syms), después los temporales.
   Cada temporal tiene un número de slot (f_temp_slot); con tempslots, varios temporales que no
   están vivos al mismo tiempo comparten el mismo slot. */
static struct symbol *f_syms[MAX_SLOTS];
static int f_nsyms;
static int f_temps[MAX_SLOTS];          /* números de los temporales (t0, t5, ...) en orden de aparición */
static int f_temp_slot[MAX_SLOTS];      /* slot de cada uno */
static int f_ntemps;
static int f_ntslots;                   /* slots de temporales usados */

static int reuse_temps;                 /* -opt tempslots */
static int debug_slots;                 /* -debug: listar los slots de cada función */

/* Argumentos acumulados por los TAC_PARAM hasta el próximo TAC_CALL */
static struct operand pending[256];
static int npending;

static const char *arg_regs[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

/* ---------- slots ---------- */

static void note_sym(struct symbol *s) {
    if (!s || s->is_global) return;
    for (int i = 0; i < f_nsyms; i++) if (f_syms[i] == s) return;
    if (f_nsyms < MAX_SLOTS) f_syms[f_nsyms++] = s;
}

static void note_opd(struct operand *o) {
    if (o->kind == OPD_VAR) {
        note_sym(o->sym);
    } else if (o->kind == OPD_TEMP) {
        for (int i = 0; i < f_ntemps; i++) if (f_temps[i] == o->temp_id) return;
        if (f_ntemps < MAX_SLOTS) f_temps[f_ntemps++] = o->temp_id;
    }
}

/* Dirección de memoria de una variable o temporal */
static void loc(struct operand *o, char *buf, size_t n) {
    if (o->kind == OPD_VAR && o->sym && o->sym->is_global) {
        snprintf(buf, n, "%s(%%rip)", o->sym->name);
    } else if (o->kind == OPD_VAR) {
        for (int i = 0; i < f_nsyms; i++)
            if (f_syms[i] == o->sym) { snprintf(buf, n, "%d(%%rbp)", -8 * (i + 1)); return; }
        snprintf(buf, n, "0(%%rbp)");   /* no debería pasar */
    } else {
        for (int i = 0; i < f_ntemps; i++)
            if (f_temps[i] == o->temp_id) {
                snprintf(buf, n, "%d(%%rbp)", -8 * (f_nsyms + f_temp_slot[i] + 1));
                return;
            }
        snprintf(buf, n, "0(%%rbp)");   /* no debería pasar */
    }
}

static int temp_index(int temp_id) {
    for (int i = 0; i < f_ntemps; i++) if (f_temps[i] == temp_id) return i;
    return -1;
}

/* ---------- reutilización de slots de temporales (-opt tempslots) ---------- */

/* Asigna slots a los temporales de las instrucciones code[0..n-1] usando una lista de slots libres:
   cuando un temporal se usa por última vez, su slot vuelve a la lista y el próximo temporal lo reutiliza.
   reused_from[i] queda con el temporal que tenía antes el slot del temporal i (-1 si el slot era nuevo).

   Vida de un temporal: desde su primera aparición hasta su último uso, con dos correcciones:
   - "param t" no carga t: el valor se lee recién en el call (gen_call), así que t vive hasta el call.
   - si un salto hacia atrás (ciclo) vuelve a un punto donde t está vivo, t vive hasta ese salto,
     porque en la próxima vuelta se lee de nuevo. Se repite hasta que no cambie (ciclos anidados). */
static void assign_temp_slots(struct tac **code, int n, int *first, int *last, int *reused_from) {
    for (int i = 0; i < f_ntemps; i++) { first[i] = -1; last[i] = -1; reused_from[i] = -1; }

    /* Primera y última aparición de cada temporal */
    for (int k = 0; k < n; k++) {
        struct operand *ops[3] = { &code[k]->a, &code[k]->b, &code[k]->res };
        for (int j = 0; j < 3; j++) {
            if (ops[j]->kind != OPD_TEMP) continue;
            int t = temp_index(ops[j]->temp_id);
            if (first[t] < 0) first[t] = k;
            int use = k;
            if (code[k]->op == TAC_PARAM)                       /* vive hasta el call que lo consume */
                for (int c = k + 1; c < n; c++)
                    if (code[c]->op == TAC_CALL) { use = c; break; }
            if (use > last[t]) last[t] = use;
        }
    }

    /* Saltos hacia atrás: extender la vida de los temporales vivos en el destino */
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int g = 0; g < n; g++) {
            if (code[g]->op != TAC_GOTO && code[g]->op != TAC_IFFALSE) continue;
            for (int p = 0; p < g; p++) {
                if (code[p]->op != TAC_LABEL || strcmp(code[p]->label, code[g]->label) != 0) continue;
                for (int t = 0; t < f_ntemps; t++)
                    if (first[t] < p && last[t] >= p && last[t] < g) { last[t] = g; changed = 1; }
            }
        }
    }

    /* Asignación con lista de libres, en el orden de las instrucciones */
    int free_slots[MAX_SLOTS], nfree = 0;
    int owner[MAX_SLOTS];                       /* último temporal que usó cada slot */
    for (int i = 0; i < f_ntemps; i++) f_temp_slot[i] = -1;
    f_ntslots = 0;

    for (int k = 0; k < n; k++) {
        /* 1. Los temporales que se usan por última vez acá liberan su slot. Se liberan antes de asignar
              el del resultado */
        for (int t = 0; t < f_ntemps; t++)
            if (last[t] == k && first[t] < k && f_temp_slot[t] >= 0)
                free_slots[nfree++] = f_temp_slot[t];

        /* 2. Los temporales que aparecen por primera vez reciben un slot (libre o nuevo) */
        struct operand *ops[3] = { &code[k]->a, &code[k]->b, &code[k]->res };
        for (int j = 0; j < 3; j++) {
            if (ops[j]->kind != OPD_TEMP) continue;
            int t = temp_index(ops[j]->temp_id);
            if (f_temp_slot[t] >= 0) continue;
            if (nfree > 0) {
                f_temp_slot[t] = free_slots[--nfree];
                reused_from[t] = owner[f_temp_slot[t]];
            } else {
                f_temp_slot[t] = f_ntslots++;
            }
            owner[f_temp_slot[t]] = f_temps[t];
            if (last[t] == k) free_slots[nfree++] = f_temp_slot[t];   /* escrito y nunca leído */
        }
    }
}

/* ---------- carga y guardado ---------- */

static void load(struct operand *o, const char *reg) {
    char b[300];
    switch (o->kind) {
        case OPD_INT:  fprintf(out, "    movq $%d, %s\n", o->ival, reg); break;
        case OPD_BOOL: fprintf(out, "    movq $%d, %s\n", o->ival ? 1 : 0, reg); break;
        case OPD_VAR:
        case OPD_TEMP:
            loc(o, b, sizeof(b));
            fprintf(out, "    movq %s, %s\n", b, reg);
            break;
        default:
            fprintf(out, "    movq $0, %s\n", reg);   /* float y otros: no soportados */
            break;
    }
}

static void store(const char *reg, struct operand *o) {
    char b[300];
    loc(o, b, sizeof(b));
    fprintf(out, "    movq %s, %s\n", reg, b);
}

/* ---------- una instrucción ---------- */

static void gen_binop(struct tac *i) {
    load(&i->a, "%r10");
    load(&i->b, "%r11");
    switch (i->bop) {
        case OP_ADD: fprintf(out, "    addq %%r11, %%r10\n"); break;
        case OP_SUB: fprintf(out, "    subq %%r11, %%r10\n"); break;
        case OP_MUL: fprintf(out, "    imulq %%r11, %%r10\n"); break;
        case OP_DIV:
        case OP_MOD:
            fprintf(out, "    movq %%r10, %%rax\n");
            fprintf(out, "    cqto\n");
            fprintf(out, "    idivq %%r11\n");
            fprintf(out, "    movq %s, %%r10\n", i->bop == OP_DIV ? "%rax" : "%rdx");
            break;
        case OP_LT:
            fprintf(out, "    cmpq %%r11, %%r10\n    setl %%al\n    movzbq %%al, %%r10\n"); break;
        case OP_GT:
            fprintf(out, "    cmpq %%r11, %%r10\n    setg %%al\n    movzbq %%al, %%r10\n"); break;
        case OP_EQ:
            fprintf(out, "    cmpq %%r11, %%r10\n    sete %%al\n    movzbq %%al, %%r10\n"); break;
        case OP_AND: fprintf(out, "    andq %%r11, %%r10\n"); break;
        case OP_OR:  fprintf(out, "    orq %%r11, %%r10\n"); break;
        default: break;
    }
    /* int es de 32 bits: el resultado aritmético se trunca y se extiende el signo (wrap como en C) */
    if (i->bop == OP_ADD || i->bop == OP_SUB || i->bop == OP_MUL ||
        i->bop == OP_DIV || i->bop == OP_MOD)
        fprintf(out, "    movslq %%r10d, %%r10\n");
    store("%r10", &i->res);
}

static void gen_call(struct tac *i) {
    int n = i->nargs;
    int nstack = n > 6 ? n - 6 : 0;
    int pad = (nstack % 2) ? 8 : 0;          /* mantener la pila alineada a 16 */

    if (pad) fprintf(out, "    subq $8, %%rsp\n");
    for (int k = n - 1; k >= 6; k--) {       /* argumentos 7.. en la pila, en orden inverso */
        load(&pending[k], "%r10");
        fprintf(out, "    pushq %%r10\n");
    }
    for (int k = 0; k < n && k < 6; k++)
        load(&pending[k], arg_regs[k]);

    fprintf(out, "    movq $0, %%rax\n");
    fprintf(out, "    call %s\n", i->label);
    if (nstack * 8 + pad > 0)
        fprintf(out, "    addq $%d, %%rsp\n", nstack * 8 + pad);
    if (i->has_result) {
        /* Una función C deja int solo en %eax y bool solo en %al: se normaliza a 64 bits */
        if (i->func && i->func->type == TYPE_BOOL)
            fprintf(out, "    movzbq %%al, %%rax\n");
        else
            fprintf(out, "    movslq %%eax, %%rax\n");
        store("%rax", &i->res);
    }
    npending = 0;
}

static void gen_instr(struct tac *i) {
    switch (i->op) {
        case TAC_ASSIGN:
            load(&i->a, "%r10");
            store("%r10", &i->res);
            break;
        case TAC_BINOP:
            gen_binop(i);
            break;
        case TAC_UNOP:
            load(&i->a, "%r10");
            if (i->bop == OP_NEG) fprintf(out, "    negq %%r10\n    movslq %%r10d, %%r10\n");
            else                  fprintf(out, "    xorq $1, %%r10\n");   /* ! sobre 0/1 */
            store("%r10", &i->res);
            break;
        case TAC_LABEL:
            fprintf(out, ".%s:\n", i->label);
            break;
        case TAC_GOTO:
            fprintf(out, "    jmp .%s\n", i->label);
            break;
        case TAC_IFFALSE:
            load(&i->a, "%r10");
            fprintf(out, "    cmpq $0, %%r10\n");
            fprintf(out, "    je .%s\n", i->label);
            break;
        case TAC_PARAM:
            if (npending < 256) pending[npending++] = i->a;
            break;
        case TAC_CALL:
            gen_call(i);
            break;
        case TAC_RETURN:
            if (i->a.kind != OPD_NONE) load(&i->a, "%rax");
            else fprintf(out, "    movq $0, %%rax\n");
            fprintf(out, "    leave\n    ret\n");
            break;
        default:
            break;
    }
}

/* ---------- función ---------- */

/* Genera la función que empieza en 'begin'. Devuelve la instrucción siguiente a su FUNC_END. */
static struct tac *gen_function(struct tac *begin) {
    f_nsyms = 0;
    f_ntemps = 0;
    npending = 0;

    /* Parámetros primero (ocupan los primeros slots) */
    for (struct node *p = begin->params; p; p = p->next) note_sym(p->sym);

    /* Resto de variables y temporales usados en el cuerpo */
    struct tac *end = begin->next;
    int n = 0;
    while (end && end->op != TAC_FUNC_END) {
        note_opd(&end->a);
        note_opd(&end->b);
        note_opd(&end->res);
        end = end->next;
        n++;
    }

    /* Slots de los temporales: uno por temporal, o reutilizados con -opt tempslots */
    struct tac **code = malloc((n > 0 ? n : 1) * sizeof(struct tac *));
    int k = 0;
    for (struct tac *i = begin->next; i != end; i = i->next) code[k++] = i;
    int first[MAX_SLOTS], last[MAX_SLOTS], reused_from[MAX_SLOTS];
    if (reuse_temps) {
        assign_temp_slots(code, n, first, last, reused_from);
    } else {
        for (int i = 0; i < f_ntemps; i++) f_temp_slot[i] = i;
        f_ntslots = f_ntemps;
    }
    free(code);

    int nslots = f_nsyms + f_ntslots;
    int frame = ((nslots * 8) + 15) / 16 * 16;    /* múltiplo de 16 */

    if (debug_slots) {
        int frame_sin = ((f_nsyms + f_ntemps) * 8 + 15) / 16 * 16;
        fprintf(stderr, "slots de %s (frame %d bytes", begin->label, frame);
        if (reuse_temps) fprintf(stderr, "; sin reutilizar: %d", frame_sin);
        fprintf(stderr, "):\n");
        for (int i = 0; i < f_nsyms; i++)
            fprintf(stderr, "  %-6s %5d(%%rbp)   %s\n", f_syms[i]->name, -8 * (i + 1),
                    f_syms[i]->kind == SYM_PARAM ? "parámetro" : "variable");
        for (int i = 0; i < f_ntemps; i++) {
            char name[16];
            snprintf(name, sizeof(name), "t%d", f_temps[i]);
            fprintf(stderr, "  %-6s %5d(%%rbp)", name, -8 * (f_nsyms + f_temp_slot[i] + 1));
            if (reuse_temps) {
                fprintf(stderr, "   vive %d-%d", first[i], last[i]);
                if (reused_from[i] >= 0) fprintf(stderr, ", reusa el slot de t%d", reused_from[i]);
            }
            fprintf(stderr, "\n");
        }
    }

    fprintf(out, "    .globl %s\n", begin->label);
    fprintf(out, "%s:\n", begin->label);
    fprintf(out, "    enter $%d, $0\n", frame);

    /* Copiar los parámetros a sus slots */
    int idx = 0;
    for (struct node *p = begin->params; p; p = p->next, idx++) {
        struct operand o = { 0 };
        o.kind = OPD_VAR;
        o.sym = p->sym;
        if (idx < 6) {
            store(arg_regs[idx], &o);
        } else {
            fprintf(out, "    movq %d(%%rbp), %%r10\n", 16 + 8 * (idx - 6));
            store("%r10", &o);
        }
    }

    for (struct tac *i = begin->next; i && i != end; i = i->next)
        gen_instr(i);

    /* Fin de la función: retorno implícito (métodos void) */
    fprintf(out, "    movq $0, %%rax\n");
    fprintf(out, "    leave\n    ret\n\n");

    return end ? end->next : NULL;
}

/* ---------- programa ---------- */

void gen_object_code(struct tac *list, FILE *o, int reuse, int debug) {
    out = o ? o : stdout;
    reuse_temps = reuse;
    debug_slots = debug;

    /* Variables globales usadas en el programa -> .bss (inicializadas en 0) */
    struct symbol *globals[MAX_SLOTS];
    int nglobals = 0;
    for (struct tac *i = list; i; i = i->next) {
        struct operand *ops[3] = { &i->a, &i->b, &i->res };
        for (int k = 0; k < 3; k++) {
            if (ops[k]->kind == OPD_VAR && ops[k]->sym && ops[k]->sym->is_global) {
                int found = 0;
                for (int g = 0; g < nglobals; g++) if (globals[g] == ops[k]->sym) found = 1;
                if (!found && nglobals < MAX_SLOTS) globals[nglobals++] = ops[k]->sym;
            }
        }
    }
    if (nglobals > 0) {
        fprintf(out, "    .bss\n");
        for (int g = 0; g < nglobals; g++) {
            fprintf(out, "    .align 8\n");
            fprintf(out, "%s:\n    .zero 8\n", globals[g]->name);
        }
    }

    fprintf(out, "    .text\n");
    for (struct tac *i = list; i; ) {
        if (i->op == TAC_FUNC_BEGIN) i = gen_function(i);
        else i = i->next;
    }

    fprintf(out, "    .section .note.GNU-stack,\"\",@progbits\n");
}