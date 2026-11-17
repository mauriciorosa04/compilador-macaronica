/* junta as fases: lexer -> parser -> semantica -> gerador.
   Sem -o so valida e mostra o relatorio. Sai com 0 se o programa e
   valido, 1 se tem erro e 2 se o problema e de uso ou de arquivo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen_llvm.h"
#include "codegen_mlir.h"
#include "erros.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

/* so apaga arquivo comum: -o /dev/null como root apagaria o /dev/null */
static void apagar_saida(const char *caminho)
{
#ifndef _WIN32
    struct stat st;
    if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode))
        return;
#endif
    remove(caminho);
}

static void uso(FILE *onde)
{
    fprintf(onde,
            "uso: macaronica [opcoes] arquivo.mac\n"
            "  -o saida.ll   gera LLVM IR em saida.ll (so se o programa for valido)\n"
            "                se o programa tem operacao vetorial, os nucleos saem\n"
            "                em MLIR no arquivo de mesmo nome com final .mlir\n"
            "  --reordena    deixa o otimizador somar reais em outra ordem nas\n"
            "                reducoes (somavet e prodesc de flut e duplocarpado)\n"
            "  --alvo=uno    confere se o programa cabe no Arduino Uno\n"
            "  --tokens      mostra os tokens antes da analise\n"
            "  --ast         mostra a arvore sintatica anotada com tipos\n"
            "  -h, --help    mostra esta ajuda\n");
}

static char *ler_arquivo(const char *caminho)
{
    FILE *f = fopen(caminho, "rb");
    if (f == NULL)
    {
        fprintf(stderr, "erro: nao foi possivel abrir '%s'\n", caminho);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (tam < 0)
    {
        fclose(f);
        fprintf(stderr, "erro: nao foi possivel ler '%s'\n", caminho);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)tam + 1);
    if (buf == NULL)
    {
        fclose(f);
        fprintf(stderr, "erro fatal: memoria insuficiente\n");
        return NULL;
    }
    size_t lidos = fread(buf, 1, (size_t)tam, f);
    buf[lidos] = '\0';
    fclose(f);

    /* byte nulo no meio cortaria o fonte calado: vira erro lexico e espaco */
    int linha = 1, coluna = 1, resto = 0;
    size_t i = 0;
    if (lidos >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        i = 3;
    for (; i < lidos; i++)
    {
        unsigned char b = (unsigned char)buf[i];
        if (b == '\0')
        {
            erro_lexico(linha, coluna, "byte nulo (0x00) no meio do arquivo");
            buf[i] = ' ';
        }
        if (b == '\n')
        {
            linha++;
            coluna = 1;
            resto = 0;
        }
        else if ((b & 0xC0) == 0x80 && resto > 0)
        {
            resto--;
        }
        else
        {
            coluna++;
            resto = (b >= 0xF0) ? 3 : (b >= 0xE0) ? 2 : (b >= 0xC0) ? 1 : 0;
        }
    }
    return buf;
}

static void imprimir_tokens(const char *fonte)
{
    Lexer lx;
    lexer_iniciar(&lx, fonte);

    /* quem reporta os erros lexicos e o parse, mais abaixo */
    erros_silenciar(1);
    printf("--- fase 1: fluxo de tokens ---\n");
    for (;;)
    {
        Token t = lexer_proximo(&lx);
        if (t.tipo == T_EOF)
            break;
        if (t.tipo == T_ERRO)
            continue;
        printf("  linha %3d, col %3d  <%s", t.linha, t.coluna,
               token_nome(t.tipo));
        if (t.tipo == T_IDENT || t.tipo == T_NUM_INT ||
            t.tipo == T_NUM_FLUT || t.tipo == T_STRING)
            printf(", \"%s\"", t.lexema);
        printf(">\n");
    }
    printf("\n");
    erros_silenciar(0);
}

int main(int argc, char **argv)
{
    int mostrar_tokens = 0, mostrar_ast = 0;
    const char *caminho = NULL;
    const char *saida = NULL;

    for (int i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        if (strcmp(a, "--tokens") == 0)
            mostrar_tokens = 1;
        else if (strcmp(a, "--ast") == 0)
            mostrar_ast = 1;
        else if (strcmp(a, "--reordena") == 0)
            mlir_permitir_reordenar(1);
        else if (strncmp(a, "--alvo=", 7) == 0)
        {
            if (!semantica_definir_alvo(a + 7))
            {
                fprintf(stderr, "erro: alvo desconhecido: %s (existem: pc, uno)\n",
                        a + 7);
                return 2;
            }
        }
        else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0)
        {
            uso(stdout);
            return 0;
        }
        else if (strcmp(a, "-o") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "erro: faltou o nome do arquivo depois de -o\n");
                uso(stderr);
                return 2;
            }
            saida = argv[++i];
        }
        else if (a[0] == '-' && a[1] != '\0')
        {
            fprintf(stderr, "erro: opcao desconhecida: %s\n", a);
            uso(stderr);
            return 2;
        }
        else if (caminho != NULL)
        {
            fprintf(stderr, "erro: mais de um arquivo de entrada ('%s' e '%s')\n",
                    caminho, a);
            uso(stderr);
            return 2;
        }
        else
            caminho = a;
    }

    if (caminho == NULL)
    {
        uso(stderr);
        return 2;
    }
    if (saida != NULL && strcmp(saida, caminho) == 0)
    {
        fprintf(stderr, "erro: a saida nao pode ser o proprio arquivo de entrada\n");
        return 2;
    }

    char *fonte = ler_arquivo(caminho);
    if (fonte == NULL)
        return 2;

    int relatorio = (saida == NULL);
    if (relatorio)
        printf("=== Front-end Macaronica: %s ===\n\n", caminho);

    if (mostrar_tokens)
        imprimir_tokens(fonte);

    Lexer lx;
    lexer_iniciar(&lx, fonte);
    Ast *programa = parse_programa(&lx);

    /* com a arvore quebrada a semantica so daria mensagem enganosa */
    if (total_erros_sintaticos() == 0)
        analisar_semantica(programa);
    else if (relatorio)
        printf("(analise semantica nao executada: corrija os erros "
               "sintaticos primeiro)\n");

    if (mostrar_ast)
    {
        printf("\n--- arvore sintatica (anotada com tipos) ---\n");
        ast_imprimir(programa, 0);
    }

    if (relatorio)
    {
        printf("\n=== RELATORIO DE VALIDACAO ===\n");
        printf("  erros lexicos:    %d\n", total_erros_lexicos());
        printf("  erros sintaticos: %d\n", total_erros_sintaticos());
        printf("  erros semanticos: %d\n", total_erros_semanticos());
        if (total_erros() == 0)
            printf(">>> ENTRADA VALIDA: lexica, sintatica e semanticamente.\n");
        else
            printf(">>> ENTRADA INVALIDA (%d erro(s) no total).\n", total_erros());
    }

    int codigo = total_erros() == 0 ? 0 : 1;
    char *saida_mlir = NULL;
    if (saida != NULL)
    {
        size_t tam = strlen(saida);
        saida_mlir = (char *)malloc(tam + 6);
        if (saida_mlir == NULL)
        {
            fprintf(stderr, "erro fatal: memoria insuficiente\n");
            return 2;
        }
        strcpy(saida_mlir, saida);
        if (tam > 3 && strcmp(saida + tam - 3, ".ll") == 0)
            saida_mlir[tam - 3] = '\0';
        strcat(saida_mlir, ".mlir");
        /* um .mlir velho seria ligado com o programa novo */
        apagar_saida(saida_mlir);

        if (total_erros() != 0)
        {
            apagar_saida(saida);
            fprintf(stderr, "macaronica: %d erro(s); '%s' nao foi gerado\n",
                    total_erros(), saida);
        }
        else
        {
            FILE *f = fopen(saida, "wb"); /* wb: \n ate no Windows */
            if (f == NULL)
            {
                fprintf(stderr, "erro: nao foi possivel criar '%s'\n", saida);
                codigo = 2;
            }
            else
            {
                gerar_llvm(programa, caminho, f);
                int falhou = ferror(f);
                if (fclose(f) != 0 || falhou)
                {
                    fprintf(stderr, "erro: falha ao gravar '%s'\n", saida);
                    codigo = 2;
                }
            }
            if (codigo == 0 && nucleos_total() > 0)
            {
                FILE *m = fopen(saida_mlir, "wb");
                if (m == NULL)
                {
                    fprintf(stderr, "erro: nao foi possivel criar '%s'\n", saida_mlir);
                    codigo = 2;
                }
                else
                {
                    gerar_mlir(caminho, m);
                    int falhou = ferror(m);
                    if (fclose(m) != 0 || falhou)
                    {
                        fprintf(stderr, "erro: falha ao gravar '%s'\n", saida_mlir);
                        codigo = 2;
                    }
                }
            }
        }
    }
    nucleos_limpar();
    free(saida_mlir);

    ast_liberar(programa);
    free(fonte);
    return codigo;
}
