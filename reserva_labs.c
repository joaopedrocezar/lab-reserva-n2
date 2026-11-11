/*
 * LabReserva - Reserva de laboratórios e equipamentos (Projeto N2 - Linguagem C)
*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARQUIVO_DADOS      "reservas_lab.dat"
#define ARQUIVO_TEMP       "reservas_lab.tmp"
#define ARQUIVO_BACKUP     "reservas_lab.bak"
#define ASSINATURA_ARQUIVO 0x4C414232 /* Identifica o formato LAB2 do arquivo. */

#define TAM_NOME           60
#define TAM_CURTO          20
#define MAX_ITENS          10
#define TURNO_MANHA        1
#define TURNO_TARDE        2
#define TURNO_NOITE        3
#define MAX_ID             1000000
#define MAX_ALUNOS         500
#define MAX_ESTOQUE        10000
#define MAX_SEMANAS_EXTRAS 30

#define FILTRO_TODAS     0
#define FILTRO_DATA      1
#define FILTRO_PROFESSOR 2

typedef struct {
    int dia, mes, ano;
} Data;

typedef enum { STATUS_CONFIRMADA = 1, STATUS_CANCELADA = 2 } StatusReserva;

typedef struct {
    int idEquipamento, quantidade;
} ItemReserva;

typedef struct {
    int  id;
    char nome[TAM_NOME], bloco[TAM_CURTO], tipo[TAM_NOME];
    int  capacidade;
} Laboratorio;

typedef struct {
    int  id;
    char nome[TAM_NOME], categoria[TAM_NOME];
    int  quantidadeTotal;
} Equipamento;

typedef struct {
    int           id;
    char          professor[TAM_NOME], matricula[TAM_CURTO], disciplina[TAM_NOME];
    int           idLaboratorio;
    Data          data;
    int           turno;
    int           qtdAlunos;
    StatusReserva status;
    int           qtdItens;
    ItemReserva   itens[MAX_ITENS];
} Reserva;

typedef struct {
    int assinatura, proximoLab, proximoEquip, proximoReserva;
    int qtdLabs, qtdEquips, qtdReservas;
} Cabecalho;

typedef struct {
    Cabecalho    info;
    Laboratorio *labs;
    Equipamento *equips;
    Reserva     *reservas;
} Sistema;

/* Leitura e validação da entrada */

void aparar(char *texto) {
    char *inicio = texto;
    char *fim;

    while (isspace((unsigned char)*inicio)) {
        inicio++;
    }
    fim = inicio + strlen(inicio);
    while (fim > inicio && isspace((unsigned char)fim[-1])) {
        fim--;
    }
    *fim = '\0';
    memmove(texto, inicio, (size_t)(fim - inicio) + 1);
}

/* Rejeita entradas maiores que o campo e descarta o restante da linha. */
int lerLinha(char *destino, int tamanho) {
    char *quebra;

    if (fgets(destino, tamanho, stdin) == NULL) {
        printf("\nEntrada encerrada.\n");
        exit(0);
    }

    quebra = strchr(destino, '\n');
    if (quebra != NULL) {
        *quebra = '\0';
    } else {
        int c = getchar();
        if (c != '\n' && c != EOF) {
            while (c != '\n' && c != EOF) {
                c = getchar();
            }
            printf("  Texto muito longo para este campo.\n");
            return 0;
        }
    }

    aparar(destino);
    return 1;
}

void lerTexto(char *pergunta, char *destino, int tamanho) {
    while (1) {
        printf("%s", pergunta);
        if (!lerLinha(destino, tamanho)) {
            continue;
        }
        if (destino[0] != '\0') {
            return;
        }
        printf("  Este campo nao pode ficar vazio.\n");
    }
}

int lerInteiro(char *pergunta, int minimo, int maximo) {
    char linha[32], sobra;
    int valor;

    while (1) {
        printf("%s", pergunta);
        if (!lerLinha(linha, 32)) {
            continue;
        }
        if (sscanf(linha, "%9d%c", &valor, &sobra) == 1 &&
            valor >= minimo && valor <= maximo) {
            return valor;
        }
        printf("  Digite um numero entre %d e %d.\n", minimo, maximo);
    }
}

int lerConfirmacao(char *pergunta) {
    char linha[8];

    while (1) {
        printf("%s (s/n): ", pergunta);
        if (!lerLinha(linha, 8)) {
            continue;
        }
        if (linha[0] != '\0' && linha[1] == '\0') {
            int resposta = tolower((unsigned char)linha[0]);
            if (resposta == 's') {
                return 1;
            }
            if (resposta == 'n') {
                return 0;
            }
        }
        printf("  Responda com s ou n.\n");
    }
}

/* Datas e turnos */

int diasNoMes(int mes, int ano) {
    int dias[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (mes == 2 && ((ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0)) {
        return 29;
    }
    return dias[mes - 1];
}

int dataValida(Data d) {
    return d.ano >= 2000 && d.ano <= 2100 && d.mes >= 1 && d.mes <= 12 &&
           d.dia >= 1 && d.dia <= diasNoMes(d.mes, d.ano);
}

int dataInt(Data d) {
    return d.ano * 10000 + d.mes * 100 + d.dia;
}

Data somarDias(Data d, int dias) {
    d.dia = d.dia + dias;
    while (d.dia > diasNoMes(d.mes, d.ano)) {
        d.dia = d.dia - diasNoMes(d.mes, d.ano);
        d.mes++;
        if (d.mes > 12) {
            d.mes = 1;
            d.ano++;
        }
    }
    return d;
}

char *nomeDiaSemana(Data d) {
    char *nomes[7] = {"Sabado", "Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta"};
    int totalDias = d.dia - 1;

    for (int ano = 2000; ano < d.ano; ano++) {
        for (int mes = 1; mes <= 12; mes++) {
            totalDias += diasNoMes(mes, ano);
        }
    }
    for (int mes = 1; mes < d.mes; mes++) {
        totalDias += diasNoMes(mes, d.ano);
    }
    return nomes[totalDias % 7];
}

int dataJaPassou(Data d) {
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);

    if (t == NULL) {
        return 0;
    }
    Data hoje = {t->tm_mday, t->tm_mon + 1, t->tm_year + 1900};
    return dataInt(d) < dataInt(hoje);
}

void lerData(char *pergunta, Data *d) {
    char linha[32], sobra;

    while (1) {
        printf("%s", pergunta);
        if (!lerLinha(linha, 32)) {
            continue;
        }
        if (sscanf(linha, "%d/%d/%d%c", &d->dia, &d->mes, &d->ano, &sobra) == 3 &&
            dataValida(*d)) {
            return;
        }
        printf("  Data invalida. Use o formato DD/MM/AAAA.\n");
    }
}

void lerDataFutura(Data *d) {
    while (1) {
        lerData("Data (DD/MM/AAAA): ", d);
        if (!dataJaPassou(*d)) {
            return;
        }
        printf("  Nao e possivel reservar em uma data que ja passou.\n");
    }
}

char *nomeTurno(int turno) {
    switch (turno) {
    case TURNO_MANHA: return "Manha";
    case TURNO_TARDE: return "Tarde";
    case TURNO_NOITE: return "Noite";
    default: return "Invalido";
    }
}

int lerTurno(void) {
    printf("\nTurno:\n");
    printf("1 - Manha\n");
    printf("2 - Tarde\n");
    printf("3 - Noite\n");
    printf("0 - Cancelar\n");
    return lerInteiro("Opcao: ", 0, 3);
}

/* Busca nos vetores */

/* Os IDs permanecem fixos mesmo quando uma exclusão muda as posições nos vetores. */
int buscarLab(Sistema *s, int id) {
    for (int i = 0; i < s->info.qtdLabs; i++) {
        if (s->labs[i].id == id) {
            return i;
        }
    }
    return -1;
}

int buscarEquip(Sistema *s, int id) {
    for (int i = 0; i < s->info.qtdEquips; i++) {
        if (s->equips[i].id == id) {
            return i;
        }
    }
    return -1;
}

int buscarReserva(Sistema *s, int id) {
    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (s->reservas[i].id == id) {
            return i;
        }
    }
    return -1;
}

char *nomeLab(Sistema *s, int id) {
    int posicao = buscarLab(s, id);

    if (posicao == -1) {
        return "(removido)";
    }
    return s->labs[posicao].nome;
}

char *nomeEquip(Sistema *s, int id) {
    int posicao = buscarEquip(s, id);

    if (posicao == -1) {
        return "(removido)";
    }
    return s->equips[posicao].nome;
}

/* Arquivo binário */

/* Grava primeiro no .tmp para não danificar o .dat se a escrita falhar. */
int salvarDados(Sistema *s) {
    FILE *f = fopen(ARQUIVO_TEMP, "wb");
    int ok = 1;

    if (f == NULL) {
        printf("  Nao foi possivel salvar os dados.\n");
        return 0;
    }

    if (fwrite(&s->info, sizeof(Cabecalho), 1, f) != 1 ||
        (s->info.qtdLabs > 0 &&
         fwrite(s->labs, sizeof(Laboratorio), (size_t)s->info.qtdLabs, f) != (size_t)s->info.qtdLabs) ||
        (s->info.qtdEquips > 0 &&
         fwrite(s->equips, sizeof(Equipamento), (size_t)s->info.qtdEquips, f) != (size_t)s->info.qtdEquips) ||
        (s->info.qtdReservas > 0 &&
         fwrite(s->reservas, sizeof(Reserva), (size_t)s->info.qtdReservas, f) != (size_t)s->info.qtdReservas)) {
        ok = 0;
    }
    if (fclose(f) != 0) {
        ok = 0;
    }
    if (!ok) {
        remove(ARQUIVO_TEMP);
        printf("  Falha ao gravar os dados.\n");
        return 0;
    }

    /* No Windows, rename não substitui o .dat existente; o .bak permite a troca. */
    if (rename(ARQUIVO_TEMP, ARQUIVO_DADOS) != 0) {
        remove(ARQUIVO_BACKUP);
        if (rename(ARQUIVO_DADOS, ARQUIVO_BACKUP) != 0) {
            remove(ARQUIVO_TEMP);
            printf("  Falha ao substituir o arquivo de dados.\n");
            return 0;
        }
        if (rename(ARQUIVO_TEMP, ARQUIVO_DADOS) != 0) {
            rename(ARQUIVO_BACKUP, ARQUIVO_DADOS);
            remove(ARQUIVO_TEMP);
            printf("  Falha ao substituir o arquivo de dados.\n");
            return 0;
        }
        remove(ARQUIVO_BACKUP);
    }
    return 1;
}

/* Retorna 0 se não houver arquivo; se estiver inválido, encerra sem sobrescrevê-lo. */
int carregarDados(Sistema *s) {
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    int ok = 1;
    long tamanhoArquivo;

    if (f == NULL) {
        if (errno != ENOENT) {
            printf("Nao foi possivel abrir o arquivo de dados.\n");
            exit(1);
        }
        /* Recupera o .bak se uma interrupção deixou o programa sem o .dat. */
        if (rename(ARQUIVO_BACKUP, ARQUIVO_DADOS) != 0) {
            if (errno != ENOENT) {
                printf("Nao foi possivel restaurar o backup.\n");
                exit(1);
            }
            return 0;
        }
        f = fopen(ARQUIVO_DADOS, "rb");
        if (f == NULL) {
            printf("Nao foi possivel recuperar o arquivo de dados.\n");
            exit(1);
        }
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        ok = 0;
    }
    tamanhoArquivo = ok ? ftell(f) : -1;
    if (tamanhoArquivo < 0 || fseek(f, 0, SEEK_SET) != 0) {
        ok = 0;
    }
    if (ok && (fread(&s->info, sizeof(Cabecalho), 1, f) != 1 ||
        s->info.assinatura != ASSINATURA_ARQUIVO ||
        s->info.qtdLabs < 0 || s->info.qtdLabs > MAX_ID ||
        s->info.qtdEquips < 0 || s->info.qtdEquips > MAX_ID ||
        s->info.qtdReservas < 0 || s->info.qtdReservas > MAX_ID ||
        s->info.proximoLab < 1 || s->info.proximoEquip < 1 || s->info.proximoReserva < 1 ||
        s->info.proximoLab > MAX_ID + 1 || s->info.proximoEquip > MAX_ID + 1 ||
        s->info.proximoReserva > MAX_ID + 1)) {
        ok = 0;
    }
    if (ok) {
        size_t esperado = sizeof(Cabecalho) +
            (size_t)s->info.qtdLabs * sizeof(Laboratorio) +
            (size_t)s->info.qtdEquips * sizeof(Equipamento) +
            (size_t)s->info.qtdReservas * sizeof(Reserva);
        if ((size_t)tamanhoArquivo != esperado) {
            ok = 0;
        }
    }
    if (ok && s->info.qtdLabs > 0) {
        s->labs = malloc((size_t)s->info.qtdLabs * sizeof(Laboratorio));
        if (s->labs == NULL ||
            fread(s->labs, sizeof(Laboratorio), (size_t)s->info.qtdLabs, f) != (size_t)s->info.qtdLabs) {
            ok = 0;
        }
    }
    if (ok && s->info.qtdEquips > 0) {
        s->equips = malloc((size_t)s->info.qtdEquips * sizeof(Equipamento));
        if (s->equips == NULL ||
            fread(s->equips, sizeof(Equipamento), (size_t)s->info.qtdEquips, f) != (size_t)s->info.qtdEquips) {
            ok = 0;
        }
    }
    if (ok && s->info.qtdReservas > 0) {
        s->reservas = malloc((size_t)s->info.qtdReservas * sizeof(Reserva));
        if (s->reservas == NULL ||
            fread(s->reservas, sizeof(Reserva), (size_t)s->info.qtdReservas, f) != (size_t)s->info.qtdReservas) {
            ok = 0;
        }
    }
    if (ok) {
        for (int i = 0; i < s->info.qtdReservas; i++) {
            Reserva *r = &s->reservas[i];
            if (r->qtdItens < 0 || r->qtdItens > MAX_ITENS || !dataValida(r->data) ||
                r->turno < TURNO_MANHA || r->turno > TURNO_NOITE || r->qtdAlunos < 1 || r->qtdAlunos > MAX_ALUNOS ||
                (r->status != STATUS_CONFIRMADA && r->status != STATUS_CANCELADA)) {
                ok = 0;
                break;
            }
            for (int j = 0; j < r->qtdItens; j++) {
                if (r->itens[j].quantidade < 1 || r->itens[j].quantidade > MAX_ESTOQUE) {
                    ok = 0;
                    break;
                }
            }
            if (!ok) {
                break;
            }
        }
    }
    fclose(f);

    if (!ok) {
        printf("Erro: o arquivo %s esta danificado ou e de outra versao do programa.\n",
               ARQUIVO_DADOS);
        printf("Guarde uma copia do arquivo antes de tentar recupera-lo.\n");
        exit(1);
    }
    return 1;
}


void erroMemoria(void) {
    printf("Erro: memoria insuficiente.\n");
    exit(1);
}

void adicionarLab(Sistema *s, Laboratorio novo) {
    Laboratorio *aumentado = realloc(s->labs, (size_t)(s->info.qtdLabs + 1) * sizeof(Laboratorio));
    if (aumentado == NULL) {
        erroMemoria();
    }
    s->labs = aumentado;
    novo.id = s->info.proximoLab++;
    s->labs[s->info.qtdLabs++] = novo;
}

void adicionarEquip(Sistema *s, Equipamento novo) {
    Equipamento *aumentado = realloc(s->equips, (size_t)(s->info.qtdEquips + 1) * sizeof(Equipamento));
    if (aumentado == NULL) {
        erroMemoria();
    }
    s->equips = aumentado;
    novo.id = s->info.proximoEquip++;
    s->equips[s->info.qtdEquips++] = novo;
}

int adicionarReserva(Sistema *s, Reserva nova) {
    Reserva *aumentado = realloc(s->reservas, (size_t)(s->info.qtdReservas + 1) * sizeof(Reserva));
    if (aumentado == NULL) {
        erroMemoria();
    }
    s->reservas = aumentado;
    nova.id = s->info.proximoReserva++;
    nova.status = STATUS_CONFIRMADA;
    s->reservas[s->info.qtdReservas++] = nova;
    return nova.id;
}


void erroMemoria(void) {
    printf("Erro: memoria insuficiente.\n");
    exit(1);
}

void adicionarLab(Sistema *s, Laboratorio novo) {
    Laboratorio *aumentado = realloc(s->labs, (size_t)(s->info.qtdLabs + 1) * sizeof(Laboratorio));
    if (aumentado == NULL) {
        erroMemoria();
    }
    s->labs = aumentado;
    novo.id = s->info.proximoLab++;
    s->labs[s->info.qtdLabs++] = novo;
}

void adicionarEquip(Sistema *s, Equipamento novo) {
    Equipamento *aumentado = realloc(s->equips, (size_t)(s->info.qtdEquips + 1) * sizeof(Equipamento));
    if (aumentado == NULL) {
        erroMemoria();
    }
    s->equips = aumentado;
    novo.id = s->info.proximoEquip++;
    s->equips[s->info.qtdEquips++] = novo;
}

int adicionarReserva(Sistema *s, Reserva nova) {
    Reserva *aumentado = realloc(s->reservas, (size_t)(s->info.qtdReservas + 1) * sizeof(Reserva));
    if (aumentado == NULL) {
        erroMemoria();
    }
    s->reservas = aumentado;
    nova.id = s->info.proximoReserva++;
    nova.status = STATUS_CONFIRMADA;
    s->reservas[s->info.qtdReservas++] = nova;
    return nova.id;
}

/* Se o salvamento falhar, os registros removidos voltam à mesma posição.
 * A remoção reduz a contagem, mas mantém espaço para restaurá-los. */
Reserva removerReserva(Sistema *s, int posicao) {
    Reserva removida = s->reservas[posicao];

    for (int i = posicao; i < s->info.qtdReservas - 1; i++) {
        s->reservas[i] = s->reservas[i + 1];
    }
    s->info.qtdReservas--;
    return removida;
}

void restaurarReserva(Sistema *s, int posicao, Reserva removida) {
    for (int i = s->info.qtdReservas; i > posicao; i--) {
        s->reservas[i] = s->reservas[i - 1];
    }
    s->reservas[posicao] = removida;
    s->info.qtdReservas++;
}

Laboratorio removerLab(Sistema *s, int posicao) {
    Laboratorio removido = s->labs[posicao];

    for (int i = posicao; i < s->info.qtdLabs - 1; i++) {
        s->labs[i] = s->labs[i + 1];
    }
    s->info.qtdLabs--;
    return removido;
}

void restaurarLab(Sistema *s, int posicao, Laboratorio removido) {
    for (int i = s->info.qtdLabs; i > posicao; i--) {
        s->labs[i] = s->labs[i - 1];
    }
    s->labs[posicao] = removido;
    s->info.qtdLabs++;
}

Equipamento removerEquip(Sistema *s, int posicao) {
    Equipamento removido = s->equips[posicao];

    for (int i = posicao; i < s->info.qtdEquips - 1; i++) {
        s->equips[i] = s->equips[i + 1];
    }
    s->info.qtdEquips--;
    return removido;
}

void restaurarEquip(Sistema *s, int posicao, Equipamento removido) {
    for (int i = s->info.qtdEquips; i > posicao; i--) {
        s->equips[i] = s->equips[i - 1];
    }
    s->equips[posicao] = removido;
    s->info.qtdEquips++;
}

/* Como novas reservas entram no fim, basta restaurar a contagem e o próximo ID. */
void desfazerInclusaoReservas(Sistema *s, int qtdAnterior, int proximoAnterior) {
    s->info.qtdReservas = qtdAnterior;
    s->info.proximoReserva = proximoAnterior;
}

void cadastrarDadosIniciais(Sistema *s) {
    Laboratorio labs[4] = {
        {0, "Lab Informatica 1", "A", "Informatica", 30},
        {0, "Lab Informatica 2", "A", "Informatica", 20},
        {0, "Lab Quimica",       "B", "Quimica",     25},
        {0, "Lab Eletronica",    "C", "Eletronica",  16}
    };
    Equipamento equips[6] = {
        {0, "Projetor",     "Audiovisual", 3},
        {0, "Notebook",     "Informatica", 15},
        {0, "Kit Arduino",  "Eletronica",  10},
        {0, "Osciloscopio", "Eletronica",  4},
        {0, "Microscopio",  "Laboratorio", 8},
        {0, "Caixa de som", "Audiovisual", 2}
    };

    for (int i = 0; i < 4; i++) {
        adicionarLab(s, labs[i]);
    }
    for (int i = 0; i < 6; i++) {
        adicionarEquip(s, equips[i]);
    }
}

/* Regras de negócio */

/* Períodos que apenas encostam não conflitam.
 * idIgnorado evita que uma edição conflite com a própria reserva. */
int ocupaTurno(Reserva *r, Data d, int turno, int idIgnorado) {
    return r->id != idIgnorado && r->status == STATUS_CONFIRMADA &&
           dataInt(r->data) == dataInt(d) && r->turno == turno;
}

int conflitoLab(Sistema *s, int idLab, Data d, int turno, int idIgnorado) {
    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (s->reservas[i].idLaboratorio == idLab &&
            ocupaTurno(&s->reservas[i], d, turno, idIgnorado)) {
            return i;
        }
    }
    return -1;
}

int qtdNaReserva(Reserva *r, int idEquip) {
    for (int i = 0; i < r->qtdItens; i++) {
        if (r->itens[i].idEquipamento == idEquip) {
            return r->itens[i].quantidade;
        }
    }
    return 0;
}

/* Calcula o maior uso simultâneo do equipamento durante o período.
 * Somar todas as reservas seria incorreto quando seus horários são diferentes. */
/* Soma as unidades reservadas no mesmo dia e turno, em qualquer laboratório. */
int unidadesEmUso(Sistema *s, int idEquip, Data d, int turno, int idIgnorado) {
    int uso = 0;

    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (ocupaTurno(&s->reservas[i], d, turno, idIgnorado)) {
            uso += qtdNaReserva(&s->reservas[i], idEquip);
            if (uso > MAX_ESTOQUE) {
                return MAX_ESTOQUE + 1;
            }
        }
    }
    return uso;
}

int unidadesLivres(Sistema *s, Equipamento *e, Data d, int turno, int idIgnorado) {
    int livres = e->quantidadeTotal - unidadesEmUso(s, e->id, d, turno, idIgnorado);
    return livres > 0 ? livres : 0;
}

int validarReserva(Sistema *s, Reserva *r, int idIgnorado) {
    int posicaoLab = buscarLab(s, r->idLaboratorio);
    int valido = 1;

    if (!dataValida(r->data) || r->turno < TURNO_MANHA || r->turno > TURNO_NOITE) {
        printf("  - Data ou turno invalido.\n");
        return 0;
    }
    if (dataJaPassou(r->data)) {
        printf("  - A data da reserva ja passou.\n");
        valido = 0;
    }
    if (r->qtdItens < 0 || r->qtdItens > MAX_ITENS ||
        r->qtdAlunos < 1 || r->qtdAlunos > MAX_ALUNOS) {
        printf("  - Quantidade de alunos ou itens invalida.\n");
        return 0;
    }
    if (posicaoLab == -1) {
        printf("  - O laboratorio escolhido nao existe mais.\n");
        return 0;
    }
    if (r->qtdAlunos > s->labs[posicaoLab].capacidade) {
        printf("  - %s comporta %d alunos, mas a turma tem %d.\n",
               s->labs[posicaoLab].nome, s->labs[posicaoLab].capacidade, r->qtdAlunos);
        valido = 0;
    }
    int posicaoConflito = conflitoLab(s, r->idLaboratorio, r->data, r->turno, idIgnorado);
    if (posicaoConflito != -1) {
        Reserva *outra = &s->reservas[posicaoConflito];
        printf("  - %s ja esta reservado nesse dia e turno para %s (reserva %d).\n",
               s->labs[posicaoLab].nome, outra->professor, outra->id);
        valido = 0;
    }
    for (int i = 0; i < r->qtdItens; i++) {
        int posicaoEquip = buscarEquip(s, r->itens[i].idEquipamento);
        if (posicaoEquip == -1) {
            printf("  - Um dos equipamentos nao existe mais.\n");
            valido = 0;
            continue;
        }
        int livres = unidadesLivres(s, &s->equips[posicaoEquip], r->data, r->turno, idIgnorado);
        if (r->itens[i].quantidade < 1 || r->itens[i].quantidade > livres) {
            printf("  - %s: pedidas %d unidades, disponiveis %d.\n",
                   s->equips[posicaoEquip].nome, r->itens[i].quantidade, livres);
            valido = 0;
        }
    }
    return valido;
}

int reservaFutura(Reserva *r) {
    return r->status == STATUS_CONFIRMADA && !dataJaPassou(r->data);
}

/* Exibição */

void imprimirTitulo(char *titulo) {
    printf("\n=== %s ===\n", titulo);
}

char *textoStatus(StatusReserva status) {
    if (status == STATUS_CONFIRMADA) {
        return "Confirmada";
    }
    return "Cancelada";
}

void imprimirCabecalhoLabs(void) {
    printf("%-4s %-28s %-7s %-20s %s\n", "ID", "Nome", "Bloco", "Tipo", "Capacidade");
}

void imprimirLab(Laboratorio *l) {
    printf("%-4d %-28.28s %-7.7s %-20.20s %d\n",
           l->id, l->nome, l->bloco, l->tipo, l->capacidade);
}

void listarLabs(Sistema *s) {
    imprimirTitulo("Laboratorios");
    if (s->info.qtdLabs == 0) {
        printf("Nenhum laboratorio cadastrado.\n");
        return;
    }
    imprimirCabecalhoLabs();
    for (int i = 0; i < s->info.qtdLabs; i++) {
        imprimirLab(&s->labs[i]);
    }
}

int listarLabsLivres(Sistema *s, Data d, int inicio, int fim, int alunos, int idIgnorado) {
    int achados = 0;

    imprimirTitulo("Laboratorios livres no horario");
    for (int i = 0; i < s->info.qtdLabs; i++) {
        if (s->labs[i].capacidade >= alunos &&
            conflitoLab(s, s->labs[i].id, d, inicio, fim, idIgnorado) == -1) {
            if (achados == 0) {
                imprimirCabecalhoLabs();
            }
            imprimirLab(&s->labs[i]);
            achados++;
        }
    }
    if (achados == 0) {
        printf("Nenhum laboratorio disponivel.\n");
    }
    return achados;
}

void listarEquips(Sistema *s) {
    imprimirTitulo("Equipamentos");
    if (s->info.qtdEquips == 0) {
        printf("Nenhum equipamento cadastrado.\n");
        return;
    }
    printf("%-4s %-28s %-20s %s\n", "ID", "Nome", "Categoria", "Quantidade");
    for (int i = 0; i < s->info.qtdEquips; i++) {
        printf("%-4d %-28.28s %-20.20s %d\n", s->equips[i].id, s->equips[i].nome,
               s->equips[i].categoria, s->equips[i].quantidadeTotal);
    }
}

void listarEquipsLivres(Sistema *s, Data d, int inicio, int fim, int idIgnorado) {
    imprimirTitulo("Equipamentos no horario");
    if (s->info.qtdEquips == 0) {
        printf("Nenhum equipamento cadastrado.\n");
        return;
    }
    printf("%-4s %-28s %-20s %s\n", "ID", "Nome", "Categoria", "Livres/Total");
    for (int i = 0; i < s->info.qtdEquips; i++) {
        printf("%-4d %-28.28s %-20.20s %d/%d\n", s->equips[i].id, s->equips[i].nome,
               s->equips[i].categoria,
               unidadesLivres(s, &s->equips[i], d, inicio, fim, idIgnorado),
               s->equips[i].quantidadeTotal);
    }
}

void exibirDetalhes(Sistema *s, Reserva *r) {
    printf("  Professor : %s (matricula %s)\n", r->professor, r->matricula);
    printf("  Disciplina: %s\n", r->disciplina);
    printf("  Lab       : %s\n", nomeLab(s, r->idLaboratorio));
    printf("  Quando    : %s, %02d/%02d/%04d, das %02d:%02d as %02d:%02d\n",
           nomeDiaSemana(r->data), r->data.dia, r->data.mes, r->data.ano,
           r->inicioMin / 60, r->inicioMin % 60, r->fimMin / 60, r->fimMin % 60);
    printf("  Alunos    : %d\n", r->qtdAlunos);
    if (r->id != 0) { /* ID zero indica uma reserva ainda não cadastrada. */
        printf("  Situacao  : %s\n", textoStatus(r->status));
    }
    printf("  Equipamentos:");
    if (r->qtdItens == 0) {
        printf(" nenhum");
    }
    printf("\n");
    for (int i = 0; i < r->qtdItens; i++) {
        printf("    - %dx %s\n", r->itens[i].quantidade,
               nomeEquip(s, r->itens[i].idEquipamento));
    }
}

/* Compara letras ASCII sem diferenciar maiúsculas e minúsculas; não trata acentos. */
int contemTexto(char *texto, char *procurado) {
    for (int i = 0; texto[i] != '\0'; i++) {
        int j = 0;

        while (procurado[j] != '\0' &&
               tolower((unsigned char)texto[i + j]) == tolower((unsigned char)procurado[j])) {
            j++;
        }
        if (procurado[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

int passaNoFiltro(Reserva *r, int filtro, Data d, char *texto) {
    if (filtro == FILTRO_DATA) {
        return r->status == STATUS_CONFIRMADA && dataInt(r->data) == dataInt(d);
    }
    if (filtro == FILTRO_PROFESSOR) {
        return contemTexto(r->professor, texto) || contemTexto(r->matricula, texto);
    }
    return 1;
}

int vemAntes(Reserva *a, Reserva *b) {
    if (dataInt(a->data) != dataInt(b->data)) {
        return dataInt(a->data) < dataInt(b->data);
    }
    if (a->inicioMin != b->inicioMin) {
        return a->inicioMin < b->inicioMin;
    }
    return a->idLaboratorio < b->idLaboratorio;
}

void listarReservas(Sistema *s, char *titulo, int filtro, Data d, char *texto) {
    int *lista = malloc((size_t)(s->info.qtdReservas + 1) * sizeof(int));
    int quantidade = 0;

    imprimirTitulo(titulo);
    if (lista == NULL) {
        printf("Memoria insuficiente para montar a lista.\n");
        return;
    }
    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (passaNoFiltro(&s->reservas[i], filtro, d, texto)) {
            lista[quantidade] = i;
            quantidade++;
        }
    }
    if (quantidade == 0) {
        printf("Nenhuma reserva encontrada.\n");
        free(lista);
        return;
    }

    /* Ordena os índices, sem mudar a ordem das reservas armazenadas. */
    for (int i = 0; i < quantidade - 1; i++) {
        for (int j = 0; j < quantidade - 1 - i; j++) {
            if (vemAntes(&s->reservas[lista[j + 1]], &s->reservas[lista[j]])) {
                int temporario = lista[j];
                lista[j] = lista[j + 1];
                lista[j + 1] = temporario;
            }
        }
    }

    printf("%-4s %-10s %-11s %-20s %-20s %-18s %-6s %s\n", "ID", "Data", "Horario",
           "Laboratorio", "Professor", "Disciplina", "Alunos", "Situacao");
    for (int i = 0; i < quantidade; i++) {
        Reserva *r = &s->reservas[lista[i]];

        printf("%-4d %02d/%02d/%04d %02d:%02d-%02d:%02d %-20.20s %-20.20s %-18.18s %-6d %s\n",
               r->id, r->data.dia, r->data.mes, r->data.ano,
               r->inicioMin / 60, r->inicioMin % 60, r->fimMin / 60, r->fimMin % 60,
               nomeLab(s, r->idLaboratorio), r->professor, r->disciplina,
               r->qtdAlunos, textoStatus(r->status));
    }
    free(lista);
}

/* CRUD de reservas */

int escolherLab(Sistema *s, Reserva *r, int idIgnorado) {
    while (1) {
        int id = lerInteiro("ID do laboratorio (0 para voltar): ", 0, MAX_ID);
        int posicao;

        if (id == 0) {
            return 0;
        }
        posicao = buscarLab(s, id);
        if (posicao == -1) {
            printf("  Laboratorio nao encontrado.\n");
        } else if (s->labs[posicao].capacidade < r->qtdAlunos) {
            printf("  Este laboratorio comporta apenas %d alunos.\n", s->labs[posicao].capacidade);
        } else if (conflitoLab(s, id, r->data, r->inicioMin, r->fimMin, idIgnorado) != -1) {
            printf("  Este laboratorio ja esta ocupado nesse horario.\n");
        } else {
            r->idLaboratorio = id;
            return 1;
        }
    }
}

/* O estoque de equipamentos é compartilhado entre os laboratórios. */
void selecionarEquips(Sistema *s, Reserva *r, int idIgnorado) {
    r->qtdItens = 0;
    if (s->info.qtdEquips == 0 || !lerConfirmacao("Deseja reservar equipamentos?")) {
        return;
    }
    listarEquipsLivres(s, r->data, r->inicioMin, r->fimMin, idIgnorado);

    while (r->qtdItens < MAX_ITENS) {
        char pergunta[64];
        int livres, posicao;
        int indice = 0;
        int id = lerInteiro("ID do equipamento (0 para concluir): ", 0, MAX_ID);

        if (id == 0) {
            return;
        }
        posicao = buscarEquip(s, id);
        if (posicao == -1) {
            printf("  Equipamento nao encontrado.\n");
            continue;
        }
        livres = unidadesLivres(s, &s->equips[posicao], r->data, r->inicioMin, r->fimMin, idIgnorado);
        if (livres == 0) {
            printf("  Nao ha unidades livres nesse horario.\n");
            continue;
        }
        while (indice < r->qtdItens && r->itens[indice].idEquipamento != id) {
            indice++;
        }
        sprintf(pergunta, "Quantidade (1 a %d): ", livres);
        r->itens[indice].idEquipamento = id;
        r->itens[indice].quantidade = lerInteiro(pergunta, 1, livres);
        if (indice == r->qtdItens) {
            r->qtdItens++;
        }
    }
    printf("  Limite de %d equipamentos por reserva atingido.\n", MAX_ITENS);
}

/* Cada semana é validada separadamente; um conflito não bloqueia as demais. */
void replicarSemanalmente(Sistema *s, Reserva base, int semanas) {
    int criadas = 0;

    for (int i = 1; i <= semanas; i++) {
        Reserva copia = base;

        copia.data = somarDias(base.data, 7 * i);
        if (!dataValida(copia.data)) {
            printf("Limite de datas atingido.\n");
            break;
        }
        printf("Semana de %02d/%02d/%04d:\n", copia.data.dia, copia.data.mes, copia.data.ano);
        if (validarReserva(s, &copia, 0)) {
            printf("  Reserva %d criada.\n", adicionarReserva(s, copia));
            criadas++;
        } else {
            printf("  Semana ignorada pelos motivos acima.\n");
        }
    }
    printf("%d de %d reservas adicionais criadas.\n", criadas, semanas);
}

void criarReserva(Sistema *s) {
    Reserva nova = {0};
    int semanas, idCriado, qtdAnterior, proximoAnterior;

    if (s->info.qtdLabs == 0) {
        printf("Cadastre um laboratorio antes de fazer reservas.\n");
        return;
    }
    nova.status = STATUS_CONFIRMADA;
    imprimirTitulo("Nova reserva");
    lerTexto("Nome do professor: ", nova.professor, TAM_NOME);
    lerTexto("Matricula: ", nova.matricula, TAM_CURTO);
    lerTexto("Disciplina: ", nova.disciplina, TAM_NOME);
    lerDataEPeriodo(&nova);
    nova.qtdAlunos = lerInteiro("Quantidade de alunos: ", 1, MAX_ALUNOS);

    if (listarLabsLivres(s, nova.data, nova.inicioMin, nova.fimMin, nova.qtdAlunos, 0) == 0 ||
        !escolherLab(s, &nova, 0)) {
        printf("Reserva nao realizada.\n");
        return;
    }
    selecionarEquips(s, &nova, 0);

    imprimirTitulo("Confira a reserva");
    exibirDetalhes(s, &nova);
    if (!lerConfirmacao("Confirmar reserva?")) {
        printf("Reserva descartada.\n");
        return;
    }
    if (!validarReserva(s, &nova, 0)) {
        printf("Reserva nao realizada.\n");
        return;
    }
    qtdAnterior = s->info.qtdReservas;
    proximoAnterior = s->info.proximoReserva;
    idCriado = adicionarReserva(s, nova);
    if (!salvarDados(s)) {
        desfazerInclusaoReservas(s, qtdAnterior, proximoAnterior);
        printf("Reserva nao realizada.\n");
        return;
    }
    printf("Reserva %d confirmada.\n", idCriado);

    semanas = lerInteiro("Repetir em quantas semanas seguintes (0 para nenhuma)? ",
                         0, MAX_SEMANAS_EXTRAS);
    if (semanas > 0) {
        /* Se a gravação falhar, desfaz só as cópias: a reserva original já está salva. */
        qtdAnterior = s->info.qtdReservas;
        proximoAnterior = s->info.proximoReserva;
        replicarSemanalmente(s, nova, semanas);
        if (!salvarDados(s)) {
            desfazerInclusaoReservas(s, qtdAnterior, proximoAnterior);
            printf("As reservas semanais nao foram criadas.\n");
        }
    }
}

void consultarReserva(Sistema *s) {
    int posicao = buscarReserva(s, lerInteiro("ID da reserva: ", 1, MAX_ID));

    if (posicao == -1) {
        printf("Reserva nao encontrada.\n");
        return;
    }
    imprimirTitulo("Detalhes da reserva");
    exibirDetalhes(s, &s->reservas[posicao]);
}

/* Só permite alterar reservas confirmadas que ainda não começaram. */
int escolherAlteravel(Sistema *s) {
    int posicao = buscarReserva(s, lerInteiro("ID da reserva: ", 1, MAX_ID));

    if (posicao == -1) {
        printf("Reserva nao encontrada.\n");
        return -1;
    }
    if (s->reservas[posicao].status == STATUS_CANCELADA) {
        printf("Esta reserva ja foi cancelada.\n");
        return -1;
    }
    if (horarioJaPassou(s->reservas[posicao].data, s->reservas[posicao].inicioMin)) {
        printf("Esta reserva ja comecou ou ja ocorreu.\n");
        return -1;
    }
    return posicao;
}

void editarReserva(Sistema *s) {
    int posicao = escolherAlteravel(s);
    Reserva c, original;

    if (posicao == -1) {
        return;
    }
    c = s->reservas[posicao];

    while (1) {
        imprimirTitulo("Editar reserva");
        exibirDetalhes(s, &c);
        printf("\n1 - Professor e matricula\n");
        printf("2 - Disciplina\n");
        printf("3 - Data e horario\n");
        printf("4 - Quantidade de alunos\n");
        printf("5 - Laboratorio\n");
        printf("6 - Equipamentos\n");
        printf("7 - Salvar alteracoes\n");
        printf("0 - Descartar alteracoes\n");

        switch (lerInteiro("Opcao: ", 0, 7)) {
        case 1:
            lerTexto("Nome do professor: ", c.professor, TAM_NOME);
            lerTexto("Matricula: ", c.matricula, TAM_CURTO);
            break;
        case 2:
            lerTexto("Disciplina: ", c.disciplina, TAM_NOME);
            break;
        case 3:
            lerDataEPeriodo(&c);
            break;
        case 4:
            c.qtdAlunos = lerInteiro("Quantidade de alunos: ", 1, MAX_ALUNOS);
            break;
        case 5:
            if (listarLabsLivres(s, c.data, c.inicioMin, c.fimMin, c.qtdAlunos, c.id) > 0) {
                escolherLab(s, &c, c.id);
            }
            break;
        case 6:
            selecionarEquips(s, &c, c.id);
            break;
        case 7:
            if (!validarReserva(s, &c, c.id)) {
                printf("Corrija os problemas acima antes de salvar.\n");
                break;
            }
            original = s->reservas[posicao];
            s->reservas[posicao] = c;
            if (salvarDados(s)) {
                printf("Reserva atualizada.\n");
                return;
            }
            s->reservas[posicao] = original;
            printf("Alteracoes nao salvas. Tente novamente ou descarte.\n");
            break;
        default:
            printf("Alteracoes descartadas.\n");
            return;
        }
    }
}

/* Cancelar mantém o histórico e libera os recursos; excluir remove o registro. */
void cancelarReserva(Sistema *s) {
    int posicao = escolherAlteravel(s);

    if (posicao == -1) {
        return;
    }
    exibirDetalhes(s, &s->reservas[posicao]);
    if (lerConfirmacao("Cancelar esta reserva?")) {
        s->reservas[posicao].status = STATUS_CANCELADA;
        if (salvarDados(s)) {
            printf("Reserva cancelada. O horario foi liberado.\n");
        } else {
            s->reservas[posicao].status = STATUS_CONFIRMADA;
            printf("Cancelamento nao realizado.\n");
        }
    }
}

void excluirReserva(Sistema *s) {
    int posicao = buscarReserva(s, lerInteiro("ID da reserva: ", 1, MAX_ID));
    Reserva removida;

    if (posicao == -1) {
        printf("Reserva nao encontrada.\n");
        return;
    }
    exibirDetalhes(s, &s->reservas[posicao]);
    if (!lerConfirmacao("Excluir definitivamente esta reserva?")) {
        return;
    }
    removida = removerReserva(s, posicao);
    if (salvarDados(s)) {
        printf("Reserva excluida.\n");
    } else {
        restaurarReserva(s, posicao, removida);
        printf("Exclusao nao realizada.\n");
    }
}

/* CRUD de laboratórios e equipamentos */

void criarLab(Sistema *s) {
    Laboratorio novo = {0};

    imprimirTitulo("Novo laboratorio");
    lerTexto("Nome: ", novo.nome, TAM_NOME);
    lerTexto("Bloco: ", novo.bloco, TAM_CURTO);
    lerTexto("Tipo: ", novo.tipo, TAM_NOME);
    novo.capacidade = lerInteiro("Capacidade de alunos: ", 1, MAX_ALUNOS);
    adicionarLab(s, novo);
    if (salvarDados(s)) {
        printf("Laboratorio cadastrado.\n");
    } else {
        s->info.qtdLabs--;
        s->info.proximoLab--;
        printf("Cadastro nao realizado.\n");
    }
}

/* A capacidade não pode ficar menor que a maior turma com reserva futura. */
void editarLab(Sistema *s) {
    int posicao = buscarLab(s, lerInteiro("ID do laboratorio: ", 1, MAX_ID));
    int maiorTurma = 0;
    int minimo = 1;
    Laboratorio *l, original;

    if (posicao == -1) {
        printf("Laboratorio nao encontrado.\n");
        return;
    }
    l = &s->labs[posicao];
    original = *l;

    for (int i = 0; i < s->info.qtdReservas; i++) {
        Reserva *r = &s->reservas[i];

        if (r->idLaboratorio == l->id && reservaFutura(r) && r->qtdAlunos > maiorTurma) {
            maiorTurma = r->qtdAlunos;
        }
    }
    lerTexto("Novo nome: ", l->nome, TAM_NOME);
    lerTexto("Novo bloco: ", l->bloco, TAM_CURTO);
    lerTexto("Novo tipo: ", l->tipo, TAM_NOME);
    if (maiorTurma > 0) {
        printf("  Existem reservas futuras com ate %d alunos.\n", maiorTurma);
        minimo = maiorTurma;
    }
    l->capacidade = lerInteiro("Nova capacidade: ", minimo, MAX_ALUNOS);
    if (salvarDados(s)) {
        printf("Laboratorio atualizado.\n");
    } else {
        *l = original;
        printf("Alteracoes nao realizadas.\n");
    }
}

void excluirLab(Sistema *s) {
    int posicao = buscarLab(s, lerInteiro("ID do laboratorio: ", 1, MAX_ID));
    Laboratorio removido;

    if (posicao == -1) {
        printf("Laboratorio nao encontrado.\n");
        return;
    }
    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (s->reservas[i].idLaboratorio == s->labs[posicao].id &&
            reservaFutura(&s->reservas[i])) {
            printf("Nao e possivel excluir: ha reservas futuras neste laboratorio.\n");
            return;
        }
    }
    if (!lerConfirmacao("Excluir este laboratorio?")) {
        return;
    }
    removido = removerLab(s, posicao);
    if (salvarDados(s)) {
        printf("Laboratorio excluido.\n");
    } else {
        restaurarLab(s, posicao, removido);
        printf("Exclusao nao realizada.\n");
    }
}

void criarEquip(Sistema *s) {
    Equipamento novo = {0};

    imprimirTitulo("Novo equipamento");
    lerTexto("Nome: ", novo.nome, TAM_NOME);
    lerTexto("Categoria: ", novo.categoria, TAM_NOME);
    novo.quantidadeTotal = lerInteiro("Quantidade total: ", 1, MAX_ESTOQUE);
    adicionarEquip(s, novo);
    if (salvarDados(s)) {
        printf("Equipamento cadastrado.\n");
    } else {
        s->info.qtdEquips--;
        s->info.proximoEquip--;
        printf("Cadastro nao realizado.\n");
    }
}

/* O total não pode ficar menor que o maior uso simultâneo em reservas futuras. */
void editarEquip(Sistema *s) {
    int posicao = buscarEquip(s, lerInteiro("ID do equipamento: ", 1, MAX_ID));
    int maiorUso = 0;
    int minimo = 1;
    Equipamento *e, original;

    if (posicao == -1) {
        printf("Equipamento nao encontrado.\n");
        return;
    }
    e = &s->equips[posicao];
    original = *e;

    for (int i = 0; i < s->info.qtdReservas; i++) {
        Reserva *r = &s->reservas[i];

        if (reservaFutura(r) && qtdNaReserva(r, e->id) > 0) {
            int uso = usoMaximo(s, e->id, r->data, r->inicioMin, r->fimMin, 0);

            if (uso > maiorUso) {
                maiorUso = uso;
            }
        }
    }
    lerTexto("Novo nome: ", e->nome, TAM_NOME);
    lerTexto("Nova categoria: ", e->categoria, TAM_NOME);
    if (maiorUso > 0) {
        printf("  Reservas futuras usam ate %d unidades ao mesmo tempo.\n", maiorUso);
        minimo = maiorUso;
    }
    e->quantidadeTotal = lerInteiro("Nova quantidade total: ", minimo, MAX_ESTOQUE);
    if (salvarDados(s)) {
        printf("Equipamento atualizado.\n");
    } else {
        *e = original;
        printf("Alteracoes nao realizadas.\n");
    }
}

void excluirEquip(Sistema *s) {
    int posicao = buscarEquip(s, lerInteiro("ID do equipamento: ", 1, MAX_ID));
    Equipamento removido;

    if (posicao == -1) {
        printf("Equipamento nao encontrado.\n");
        return;
    }
    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (reservaFutura(&s->reservas[i]) &&
            qtdNaReserva(&s->reservas[i], s->equips[posicao].id) > 0) {
            printf("Nao e possivel excluir: ha reservas futuras com este equipamento.\n");
            return;
        }
    }
    if (!lerConfirmacao("Excluir este equipamento?")) {
        return;
    }
    removido = removerEquip(s, posicao);
    if (salvarDados(s)) {
        printf("Equipamento excluido.\n");
    } else {
        restaurarEquip(s, posicao, removido);
        printf("Exclusao nao realizada.\n");
    }
}

/* Consultas e relatórios */

void listarTodasReservas(Sistema *s) {
    Data semData = {0, 0, 0};

    listarReservas(s, "Todas as reservas", FILTRO_TODAS, semData, "");
}

void exibirAgendaDoDia(Sistema *s) {
    Data d;
    char titulo[64];

    lerData("Data (DD/MM/AAAA): ", &d);
    sprintf(titulo, "Agenda de %s, %02d/%02d/%04d", nomeDiaSemana(d), d.dia, d.mes, d.ano);
    listarReservas(s, titulo, FILTRO_DATA, d, "");
}

void buscarPorProfessor(Sistema *s) {
    char procurado[TAM_NOME];
    Data semData = {0, 0, 0};

    lerTexto("Nome ou matricula: ", procurado, TAM_NOME);
    listarReservas(s, "Reservas encontradas", FILTRO_PROFESSOR, semData, procurado);
}

void consultarDisponibilidade(Sistema *s) {
    Data d;
    int inicio, fim;

    lerData("Data (DD/MM/AAAA): ", &d);
    lerPeriodo(&inicio, &fim);
    listarLabsLivres(s, d, inicio, fim, 0, 0);
    listarEquipsLivres(s, d, inicio, fim, 0);
}

void exibirRelatorio(Sistema *s) {
    int confirmadas = 0;
    int canceladas = 0;
        int *minutos = calloc((size_t)s->info.qtdLabs + 1, sizeof(int));
    int *unidades = calloc((size_t)s->info.qtdEquips + 1, sizeof(int));

    if (minutos == NULL || unidades == NULL) {
        printf("Memoria insuficiente para gerar o relatorio.\n");
        free(minutos);
        free(unidades);
        return;
    }
    for (int i = 0; i < s->info.qtdReservas; i++) {
        Reserva *r = &s->reservas[i];

        if (r->status != STATUS_CONFIRMADA) {
            canceladas++;
            continue;
        }
        confirmadas++;
        int posLab = buscarLab(s, r->idLaboratorio);
        if (posLab != -1) {
            minutos[posLab] += r->fimMin - r->inicioMin;
        }
        for (int j = 0; j < r->qtdItens; j++) {
            int posEquip = buscarEquip(s, r->itens[j].idEquipamento);
            if (posEquip != -1) {
                unidades[posEquip] += r->itens[j].quantidade;
            }
        }
    }

    imprimirTitulo("Relatorio de uso");
    printf("Reservas confirmadas: %d\n", confirmadas);
    printf("Reservas canceladas : %d\n", canceladas);

    imprimirTitulo("Horas reservadas por laboratorio");
    for (int i = 0; i < s->info.qtdLabs; i++) {
        printf("%-22.22s %d h %02d min\n", s->labs[i].nome, minutos[i] / 60, minutos[i] % 60);
    }

    imprimirTitulo("Total de unidades solicitadas por equipamento");
    for (int i = 0; i < s->info.qtdEquips; i++) {
        printf("%-22.22s %d unidades\n", s->equips[i].nome, unidades[i]);
    }
    free(minutos);
    free(unidades);
}

/* Menus */

void menuReservas(Sistema *s) {
    while (1) {
        imprimirTitulo("Reservas");
        printf("1 - Nova reserva\n");
        printf("2 - Listar reservas\n");
        printf("3 - Consultar reserva\n");
        printf("4 - Editar reserva\n");
        printf("5 - Cancelar reserva\n");
        printf("6 - Excluir reserva\n");
        printf("0 - Voltar\n");

        switch (lerInteiro("Opcao: ", 0, 6)) {
        case 1: criarReserva(s); break;
        case 2: listarTodasReservas(s); break;
        case 3: consultarReserva(s); break;
        case 4: editarReserva(s); break;
        case 5: cancelarReserva(s); break;
        case 6: excluirReserva(s); break;
        default: return;
        }
    }
}

void menuLabs(Sistema *s) {
    while (1) {
        imprimirTitulo("Laboratorios");
        printf("1 - Cadastrar\n");
        printf("2 - Listar\n");
        printf("3 - Editar\n");
        printf("4 - Excluir\n");
        printf("0 - Voltar\n");

        switch (lerInteiro("Opcao: ", 0, 4)) {
        case 1: criarLab(s); break;
        case 2: listarLabs(s); break;
        case 3: editarLab(s); break;
        case 4: excluirLab(s); break;
        default: return;
        }
    }
}

void menuEquips(Sistema *s) {
    while (1) {
        imprimirTitulo("Equipamentos");
        printf("1 - Cadastrar\n");
        printf("2 - Listar\n");
        printf("3 - Editar\n");
        printf("4 - Excluir\n");
        printf("0 - Voltar\n");

        switch (lerInteiro("Opcao: ", 0, 4)) {
        case 1: criarEquip(s); break;
        case 2: listarEquips(s); break;
        case 3: editarEquip(s); break;
        case 4: excluirEquip(s); break;
        default: return;
        }
    }
}

void menuConsultas(Sistema *s) {
    while (1) {
        imprimirTitulo("Consultas e relatorios");
        printf("1 - Agenda do dia\n");
        printf("2 - Buscar por professor\n");
        printf("3 - Consultar disponibilidade\n");
        printf("4 - Relatorio de uso\n");
        printf("0 - Voltar\n");

        switch (lerInteiro("Opcao: ", 0, 4)) {
        case 1: exibirAgendaDoDia(s); break;
        case 2: buscarPorProfessor(s); break;
        case 3: consultarDisponibilidade(s); break;
        case 4: exibirRelatorio(s); break;
        default: return;
        }
    }
}

int main(void) {
    Sistema s = {0};
    int executando = 1;

    s.info.assinatura = ASSINATURA_ARQUIVO;
    s.info.proximoLab = 1;
    s.info.proximoEquip = 1;
    s.info.proximoReserva = 1;

    if (!carregarDados(&s)) {
        cadastrarDadosIniciais(&s);
        if (!salvarDados(&s)) {
            free(s.labs);
            free(s.equips);
            return 1;
        }
    }

    printf("==============================================\n");
    printf("  LabReserva - Reserva de laboratorios\n");
    printf("==============================================\n");

    while (executando) {
        imprimirTitulo("Menu principal");
        printf("1 - Reservas\n");
        printf("2 - Laboratorios\n");
        printf("3 - Equipamentos\n");
        printf("4 - Consultas e relatorios\n");
        printf("0 - Sair\n");

        switch (lerInteiro("Opcao: ", 0, 4)) {
        case 1: menuReservas(&s); break;
        case 2: menuLabs(&s); break;
        case 3: menuEquips(&s); break;
        case 4: menuConsultas(&s); break;
        default: executando = 0; break;
        }
    }

    free(s.labs);
    free(s.equips);
    free(s.reservas);
    printf("Ate logo!\n");
    return 0;
}
