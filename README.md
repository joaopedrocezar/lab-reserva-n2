
# LabReserva

Sistema de gerenciamento de reservas de laboratórios e equipamentos acadêmicos, desenvolvido em linguagem C para a disciplina de Programação Estruturada do curso de Engenharia da Computação da Universidade São Francisco (USF).

## Sobre o projeto

O LabReserva foi desenvolvido para auxiliar no gerenciamento de laboratórios e equipamentos de uma instituição de ensino.

O sistema permite cadastrar recursos, organizar reservas e consultar a disponibilidade de laboratórios e equipamentos em diferentes datas e turnos (manhã, tarde e noite).

A aplicação funciona pelo terminal e utiliza arquivos binários para armazenar os dados entre as execuções.

## Funcionalidades

### Laboratórios
- Cadastro de laboratórios com nome, bloco, tipo e capacidade.
- Listagem dos laboratórios cadastrados.
- Edição e exclusão de registros.
- Consulta de disponibilidade por data e turno.
- Verificação de conflitos entre reservas.

### Equipamentos
- Cadastro de equipamentos com nome, categoria e quantidade.
- Listagem, edição e exclusão de equipamentos.
- Controle de estoque compartilhado entre laboratórios.
- Consulta da quantidade disponível em determinada data e turno.
- Validação do estoque compartilhado entre reservas do mesmo turno.

### Reservas
- Criação de reservas vinculadas a professores e disciplinas.
- Seleção de laboratório e equipamentos.
- Consulta, edição, cancelamento e exclusão de reservas.
- Validação progressiva de data, turno, capacidade e disponibilidade dos laboratórios.
- Repetição semanal com aviso das datas indisponíveis e confirmação única.
- Histórico de reservas confirmadas e canceladas.

### Consultas e relatórios
- Consulta da agenda diária.
- Pesquisa de reservas por professor ou matrícula.
- Verificação de disponibilidade de recursos.
- Relatórios de utilização de laboratórios e equipamentos.

## Tecnologias e conceitos

O projeto foi desenvolvido em **linguagem C**, aplicando conceitos estudados na disciplina:

- Programação estruturada e modularização com funções.
- Estruturas de dados utilizando `struct`.
- Ponteiros e passagem de parâmetros por referência.
- Alocação dinâmica de memória com `malloc`, `calloc` e `realloc`.
- Gerenciamento de memória com `free`.
- Manipulação de strings.
- Vetores, buscas e ordenação.
- Estruturas condicionais e de repetição.
- Manipulação de arquivos binários.

## Como executar

### Requisitos

- Compilador C, como GCC.
- Terminal ou prompt de comando.

### Compilação

No Linux:

```bash
gcc -std=c11 reserva_labs.c -o labreserva
./labreserva
```

No Windows, utilizando GCC:

```powershell
gcc -std=c11 reserva_labs.c -o labreserva.exe
.\labreserva.exe
```

### Armazenamento dos dados

O programa utiliza o arquivo `reservas_lab.dat` para armazenar os registros.

Na primeira execução, alguns laboratórios e equipamentos de exemplo são cadastrados automaticamente.

O formato de dados foi atualizado para a versão LAB2. Como o projeto ainda usa somente dados de teste, apague o arquivo `reservas_lab.dat` antigo antes de executar esta versão, se já tiver usado o programa anteriormente. O programa recriará os cadastros de exemplo. Arquivos `.tmp` e `.bak` são auxiliares do salvamento.

O arquivo de dados é criado no diretório de execução e não precisa ser enviado ao repositório.

## Estrutura do projeto

```text
lab-reserva-n2/
├── reserva_labs.c
├── README.md
├── .github/workflows/validate-labreserva.yml
└── .gitignore
```

## Desenvolvimento

O projeto está em desenvolvimento e poderá receber ajustes nas funcionalidades e na organização do código.

A reserva sempre inclui um laboratório; a solicitação de equipamentos é opcional.

## Contexto acadêmico

**Instituição:** Universidade São Francisco (USF)  
**Curso:** Engenharia da Computação  
**Disciplina:** Programação Estruturada  
**Avaliação:** N2  
**Linguagem:** C
