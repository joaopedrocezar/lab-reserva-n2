/*
 * LabReserva - Reserva de laboratórios e equipamentos (Projeto N2 - Linguagem C)
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARQUIVO_DADOS      "reservas_lab.dat"
#define ARQUIVO_TEMP       "reservas_lab.tmp"
#define ARQUIVO_BACKUP     "reservas_lab.bak"
#define ASSINATURA_ARQUIVO 0x4C414231

#define TAM_NOME           60
#define TAM_CURTO          20
#define MAX_ITENS          10
#define HORA_ABERTURA      (7 * 60)  /* Em minutos desde a meia-noite */
#define HORA_FECHAMENTO    (23 * 60)
#define MAX_ID             1000000
#define MAX_ALUNOS         500
#define MAX_ESTOQUE        10000
#define MAX_SEMANAS_EXTRAS 30

/* Tipos de filtro usados na listagem de reservas */
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
    int           inicioMin, fimMin; /* Em minutos desde a meia-noite */
    int           qtdAlunos;
    StatusReserva status;
    int           qtdItens;
    ItemReserva   itens[MAX_ITENS];
} Reserva;

/* Gravado no início do arquivo de dados. */
typedef struct {
    int assinatura, proximoLab, proximoEquip, proximoReserva;
    int qtdLabs, qtdEquips, qtdReservas;
} Cabecalho;

/* Todos os dados do programa; é passado por ponteiro, sem variáveis globais. */
typedef struct {
    Cabecalho    info;
    Laboratorio *labs;
    Equipamento *equips;
    Reserva     *reservas;
} Sistema;

/* Leitura e validação da entrada */

/* Remove os espaços do início e do fim de um texto. */
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

/* Devolve 0 quando a linha nao cabe no campo informado. */
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

/* Lê um inteiro entre minimo e maximo, repetindo enquanto for inválido. */
int lerInteiro(char *pergunta, int minimo, int maximo) {
    char linha[32], sobra;
    int valor;

    while (1) {
        printf("%s", pergunta);
        if (!lerLinha(linha, 32)) {
            continue;
        }
        /* %c detecta caracteres extras apos o numero. */
        if (sscanf(linha, "%9d%c", &valor, &sobra) == 1 &&
            valor >= minimo && valor <= maximo) {
            return valor;
        }
        printf("  Digite um numero entre %d e %d.\n", minimo, maximo);
    }
}

/* Retorna 1 para sim e 0 para não. */
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

/* Datas e horários */

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

/* Converte a data em AAAAMMDD, que pode ser comparado diretamente. */
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

/* Conta os dias desde 01/01/2000, que foi um sábado. */
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

/* Compara o momento informado com o relógio do computador. */
int horarioJaPassou(Data d, int minutos) {
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    int hoje;

    if (t == NULL) {
        return 0; /* Sem relógio confiável, a verificação não é aplicada */
    }
    hoje = (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday;

    if (dataInt(d) != hoje) {
        return dataInt(d) < hoje;
    }
    return minutos <= t->tm_hour * 60 + t->tm_min;
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

/* Lê um horário HH:MM e devolve os minutos desde a meia-noite. */
int lerHorario(char *pergunta) {
    char linha[32], sobra;
    int hora, minuto;

    while (1) {
        printf("%s", pergunta);
        if (!lerLinha(linha, 32)) {
            continue;
        }
        if (sscanf(linha, "%d:%d%c", &hora, &minuto, &sobra) == 2 &&
            hora >= 0 && hora <= 23 && minuto >= 0 && minuto <= 59) {
            return hora * 60 + minuto;
        }
        printf("  Horario invalido. Use o formato HH:MM.\n");
    }
}

void lerPeriodo(int *inicio, int *fim) {
    while (1) {
        *inicio = lerHorario("Horario de inicio (HH:MM): ");
        *fim = lerHorario("Horario de fim (HH:MM): ");
        if (*inicio >= *fim) {
            printf("  O fim deve ser depois do inicio.\n");
        } else if (*inicio < HORA_ABERTURA || *fim > HORA_FECHAMENTO) {
            printf("  Os laboratorios funcionam das 07:00 as 23:00.\n");
        } else {
            return;
        }
    }
}

void lerDataEPeriodo(Reserva *r) {
    while (1) {
        lerData("Data (DD/MM/AAAA): ", &r->data);
        lerPeriodo(&r->inicioMin, &r->fimMin);
        if (!horarioJaPassou(r->data, r->inicioMin)) {
            return;
        }
        printf("  Nao e possivel reservar um horario que ja passou.\n");
    }
}

/* Busca nos vetores */

/* As três buscas retornam a posição no vetor, ou -1 se o ID não existir. */
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

/* Grava primeiro em um arquivo temporario para nao perder o arquivo anterior. */
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

    /* Em alguns sistemas o rename nao substitui um arquivo existente. */
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

/* Retorna 0 se o arquivo não existe. Se estiver danificado, encerra sem alterá-lo. */
int carregarDados(Sistema *s) {
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    int ok = 1;
    long tamanhoArquivo;

    if (f == NULL) {
        return 0;
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
                r->inicioMin < HORA_ABERTURA || r->fimMin > HORA_FECHAMENTO ||
                r->inicioMin >= r->fimMin || r->qtdAlunos < 1 || r->qtdAlunos > MAX_ALUNOS ||
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

/* Inclusão nos vetores */

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

/* Inclui a reserva como confirmada e retorna o ID gerado. */
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

/* Cadastra exemplos no primeiro uso, para testar o sistema imediatamente. */
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

/*
 * Retorna 1 se a reserva está confirmada e coincide com o período. Períodos
 * apenas encostados não coincidem. idIgnorado evita que uma reserva em edição
 * entre em conflito com ela mesma.
 */
int ocupaPeriodo(Reserva *r, Data d, int inicio, int fim, int idIgnorado) {
    return r->id != idIgnorado && r->status == STATUS_CONFIRMADA &&
           dataInt(r->data) == dataInt(d) && r->inicioMin < fim && inicio < r->fimMin;
}

/* Retorna a posição da reserva que já ocupa o laboratório, ou -1 se estiver livre. */
int conflitoLab(Sistema *s, int idLab, Data d, int inicio, int fim, int idIgnorado) {
    for (int i = 0; i < s->info.qtdReservas; i++) {
        if (s->reservas[i].idLaboratorio == idLab &&
            ocupaPeriodo(&s->reservas[i], d, inicio, fim, idIgnorado)) {
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

/*
 * Maior número de unidades em uso ao mesmo tempo durante o período. Somar as
 * reservas seria errado, pois aulas em sequência podem usar as mesmas unidades.
 * Por isso, o uso é calculado minuto a minuto, guardando o maior valor.
 */
int usoMaximo(Sistema *s, int idEquip, Data d, int inicio, int fim, int idIgnorado) {
    int maior = 0;

    for (int minuto = inicio; minuto < fim; minuto++) {
        int uso = 0;

        for (int i = 0; i < s->info.qtdReservas; i++) {
            if (ocupaPeriodo(&s->reservas[i], d, minuto, minuto + 1, idIgnorado)) {
                uso += qtdNaReserva(&s->reservas[i], idEquip);
            }
        }
        if (uso > maior) {
            maior = uso;
        }
    }
    return maior;
}

int unidadesLivres(Sistema *s, Equipamento *e, Data d, int inicio, int fim, int idIgnorado) {
    int livres = e->quantidadeTotal - usoMaximo(s, e->id, d, inicio, fim, idIgnorado);

    if (livres < 0) {
        return 0;
    }
    return livres;
}

/* Confere todas as regras e explica cada problema. Retorna 1 se a reserva é válida. */
int validarReserva(Sistema *s, Reserva *r, int idIgnorado) {
    int posicaoLab = buscarLab(s, r->idLaboratorio);
    int posicaoConflito;
    int valida = 1;

    if (!dataValida(r->data) || r->inicioMin < HORA_ABERTURA ||
        r->fimMin > HORA_FECHAMENTO || r->inicioMin >= r->fimMin) {
        printf("  - Data ou horario fora dos limites permitidos.\n");
        return 0;
    }
    if (r->qtdItens < 0 || r->qtdItens > MAX_ITENS ||
        r->qtdAlunos < 1 || r->qtdAlunos > MAX_ALUNOS) {
        printf("  - Quantidade de alunos ou itens invalida.\n");
        return 0;
    }
    if (horarioJaPassou(r->data, r->inicioMin)) {
        printf("  - O horario informado ja passou.\n");
        valida = 0;
    }
    if (posicaoLab == -1) {
        printf("  - O laboratorio escolhido nao existe mais.\n");
        return 0;
    }
    if (r->qtdAlunos > s->labs[posicaoLab].capacidade) {
        printf("  - %s comporta %d alunos, mas a turma tem %d.\n",
               s->labs[posicaoLab].nome, s->labs[posicaoLab].capacidade, r->qtdAlunos);
        valida = 0;
    }

    posicaoConflito = conflitoLab(s, r->idLaboratorio, r->data, r->inicioMin, r->fimMin, idIgnorado);
    if (posicaoConflito != -1) {
        Reserva *conflito = &s->reservas[posicaoConflito];

        printf("  - %s ja esta reservado das %02d:%02d as %02d:%02d para %s (reserva %d).\n",
               s->labs[posicaoLab].nome,
               conflito->inicioMin / 60, conflito->inicioMin % 60,
               conflito->fimMin / 60, conflito->fimMin % 60,
               conflito->professor, conflito->id);
        valida = 0;
    }

    for (int i = 0; i < r->qtdItens; i++) {
        int posicaoEquip = buscarEquip(s, r->itens[i].idEquipamento);

        if (posicaoEquip == -1) {
            printf("  - Um dos equipamentos nao existe mais.\n");
            valida = 0;
        } else {
            int livres = unidadesLivres(s, &s->equips[posicaoEquip], r->data,
                                        r->inicioMin, r->fimMin, idIgnorado);

            if (r->itens[i].quantidade < 1) {
                printf("  - Quantidade invalida para %s.\n", s->equips[posicaoEquip].nome);
                valida = 0;
            }
            if (r->itens[i].quantidade > livres) {
                printf("  - %s: pedidas %d unidades, disponiveis %d.\n",
                       s->equips[posicaoEquip].nome, r->itens[i].quantidade, livres);
                valida = 0;
            }
        }
    }
    return valida;
}

/* Retorna 1 se a reserva está confirmada e ainda não começou. */
int reservaFutura(Reserva *r) {
    return r->status == STATUS_CONFIRMADA && !horarioJaPassou(r->data, r->inicioMin);
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

/* Lista os laboratórios livres que comportam a turma. Retorna quantos exibiu. */
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

/* Mostra cada equipamento com as unidades livres no período. */
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
    if (r->id != 0) { /* Reserva sem ID ainda nao foi confirmada. */
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

/* Procura um texto dentro de outro, sem diferenciar maiúsculas de minúsculas. */
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

/* Retorna 1 se a reserva deve aparecer na listagem pedida. */
int passaNoFiltro(Reserva *r, int filtro, Data d, char *texto) {
    if (filtro == FILTRO_DATA) {
        return r->status == STATUS_CONFIRMADA && dataInt(r->data) == dataInt(d);
    }
    if (filtro == FILTRO_PROFESSOR) {
        return contemTexto(r->professor, texto) || contemTexto(r->matricula, texto);
    }
    return 1;
}

/* Retorna 1 se a reserva a deve aparecer antes da b: data, horário e laboratório. */
int vemAntes(Reserva *a, Reserva *b) {
    if (dataInt(a->data) != dataInt(b->data)) {
        return dataInt(a->data) < dataInt(b->data);
    }
    if (a->inicioMin != b->inicioMin) {
        return a->inicioMin < b->inicioMin;
    }
    return a->idLaboratorio < b->idLaboratorio;
}

/* Exibe em ordem cronológica as reservas que passam no filtro. */
void listarReservas(Sistema *s, char *titulo, int filtro, Data d, char *texto) {
    int *lista = malloc((size_t)(s->info.qtdReservas + 1) * sizeof(int)); /* Indices selecionados */
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

    /* Ordenacao por data e horario. */
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

/* Pede um laboratório livre que comporte a turma. Retorna 0 se o usuário desistir. */
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

/* Define os equipamentos da reserva, limitando cada pedido às unidades livres. */
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
        /* Pedir o mesmo equipamento de novo corrige a quantidade anterior */
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

/* Copia a reserva para as semanas seguintes, pulando as que tiverem conflito. */
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
    int semanas;

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
    int idCriado = adicionarReserva(s, nova);
    if (!salvarDados(s)) {
        printf("A reserva %d ficou na memoria, mas nao foi salva no arquivo.\n", idCriado);
        return;
    }
    printf("Reserva %d confirmada.\n", idCriado);

    semanas = lerInteiro("Repetir em quantas semanas seguintes (0 para nenhuma)? ",
                         0, MAX_SEMANAS_EXTRAS);
    if (semanas > 0) {
        replicarSemanalmente(s, nova, semanas);
        salvarDados(s);
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

/* Retorna a posição da reserva, se estiver confirmada e não tiver começado; senão, -1. */
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

/* Altera uma cópia; o original só muda ao salvar com todas as regras atendidas. */
void editarReserva(Sistema *s) {
    int posicao = escolherAlteravel(s);
    Reserva c;

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
            if (validarReserva(s, &c, c.id)) {
                s->reservas[posicao] = c;
                salvarDados(s);
                printf("Reserva atualizada.\n");
                return;
            }
            printf("Corrija os problemas acima antes de salvar.\n");
            break;
        default:
            printf("Alteracoes descartadas.\n");
            return;
        }
    }
}

/* Cancelar libera o horário, mas mantém a reserva no histórico e no relatório. */
void cancelarReserva(Sistema *s) {
    int posicao = escolherAlteravel(s);

    if (posicao == -1) {
        return;
    }
    exibirDetalhes(s, &s->reservas[posicao]);
    if (lerConfirmacao("Cancelar esta reserva?")) {
        s->reservas[posicao].status = STATUS_CANCELADA;
        salvarDados(s);
        printf("Reserva cancelada. O horario foi liberado.\n");
    }
}

/* Excluir apaga a reserva do sistema, sem deixar registro. */
void excluirReserva(Sistema *s) {
    int posicao = buscarReserva(s, lerInteiro("ID da reserva: ", 1, MAX_ID));

    if (posicao == -1) {
        printf("Reserva nao encontrada.\n");
        return;
    }
    exibirDetalhes(s, &s->reservas[posicao]);
    if (!lerConfirmacao("Excluir definitivamente esta reserva?")) {
        return;
    }
    /* Desloca as reservas seguintes para não deixar buraco no vetor */
    for (int i = posicao; i < s->info.qtdReservas - 1; i++) {
        s->reservas[i] = s->reservas[i + 1];
    }
    s->info.qtdReservas--;
    salvarDados(s);
    printf("Reserva excluida.\n");
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
    salvarDados(s);
    printf("Laboratorio cadastrado.\n");
}

/* A capacidade não pode ficar menor que a maior turma com reserva futura. */
void editarLab(Sistema *s) {
    int posicao = buscarLab(s, lerInteiro("ID do laboratorio: ", 1, MAX_ID));
    int maiorTurma = 0;
    int minimo = 1;
    Laboratorio *l;

    if (posicao == -1) {
        printf("Laboratorio nao encontrado.\n");
        return;
    }
    l = &s->labs[posicao];

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
    salvarDados(s);
    printf("Laboratorio atualizado.\n");
}

void excluirLab(Sistema *s) {
    int posicao = buscarLab(s, lerInteiro("ID do laboratorio: ", 1, MAX_ID));

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
    for (int i = posicao; i < s->info.qtdLabs - 1; i++) {
        s->labs[i] = s->labs[i + 1];
    }
    s->info.qtdLabs--;
    salvarDados(s);
    printf("Laboratorio excluido.\n");
}

void criarEquip(Sistema *s) {
    Equipamento novo = {0};

    imprimirTitulo("Novo equipamento");
    lerTexto("Nome: ", novo.nome, TAM_NOME);
    lerTexto("Categoria: ", novo.categoria, TAM_NOME);
    novo.quantidadeTotal = lerInteiro("Quantidade total: ", 1, MAX_ESTOQUE);
    adicionarEquip(s, novo);
    salvarDados(s);
    printf("Equipamento cadastrado.\n");
}

/* O total não pode ficar menor que o maior uso simultâneo em reservas futuras. */
void editarEquip(Sistema *s) {
    int posicao = buscarEquip(s, lerInteiro("ID do equipamento: ", 1, MAX_ID));
    int maiorUso = 0;
    int minimo = 1;
    Equipamento *e;

    if (posicao == -1) {
        printf("Equipamento nao encontrado.\n");
        return;
    }
    e = &s->equips[posicao];

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
    salvarDados(s);
    printf("Equipamento atualizado.\n");
}

void excluirEquip(Sistema *s) {
    int posicao = buscarEquip(s, lerInteiro("ID do equipamento: ", 1, MAX_ID));

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
    for (int i = posicao; i < s->info.qtdEquips - 1; i++) {
        s->equips[i] = s->equips[i + 1];
    }
    s->info.qtdEquips--;
    salvarDados(s);
    printf("Equipamento excluido.\n");
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
    /* calloc inicia os contadores em zero. */
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
        salvarDados(&s);
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
