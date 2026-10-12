/* JOSIAH'S CRAZY CALCULATOR INTERPRETER (C) 2026 */

/*
This is a simple calculator interpreter.
It can do addition, subtraction, multiplication, division, and exponentiation of whole or fractional numbers.

It works in 4 stages: first it collects user input, then it lexes the input into tokens,
and then it parses those tokens into an abstract syntax tree. Finally, it evaluates this tree to a final number.

The lexer I rolled myself (which is why it's a mess), but for the syntax parsing I had to look up how to do that stuff.
I ended up going with a recursive descent parser because it was the simplest to reason about as a beginner.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

/*              INPUT               */
static char *read_input(void)
{
    printf("> ");
    // Read user input using a completely overengineered dynamically allocated string buffer
    size_t cap_grow = 8; // The number of bytes the input buffer grows by
                         // when it approaches capacity
    size_t capacity = cap_grow;
    size_t len = 0;
    int char_buf;

    char *input_buf = malloc(capacity);
    if (!input_buf)
    {
        fprintf(stderr, "We couldn't allocate the memory. ");
        fprintf(stderr, "What the fuck is wrong with your computer? ");
        fprintf(stderr, "I can't believe this...\n");
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        char_buf = getchar();

        if (char_buf == '\n' || char_buf == EOF)
        {
            input_buf[len] = '\0';
            break;
        }

        input_buf[len] = (char)char_buf;
        len++;

        if (len >= capacity - 1)
        {
            capacity += cap_grow;
            char *tmp = realloc(input_buf, capacity); // I'M BEING REALLY CAREFUL, GUYS!!!
            if (!tmp)
            {
                fprintf(stderr, "We couldn't allocate more memory. Fuck this...\n");
                exit(EXIT_FAILURE);
            }
            input_buf = tmp;
        }
    }

    return input_buf;
}

/*              TOKENS              */
typedef enum token_type
{
    TOK_PLUS,
    TOK_MINUS,
    TOK_DIV,
    TOK_MULT,
    TOK_EXP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_NUMBER
} TokenType;

typedef struct token
{
    TokenType type;
    double number;
} Token;

static Token make_token(TokenType type)
{
    return (Token){.type = type};
}

static Token make_number(double number)
{
    return (Token){.type = TOK_NUMBER, .number = number};
}

typedef struct token_buffer
{
    size_t cap_grow;
    size_t capacity;
    size_t len;
    Token *data;
} TokenBuffer;

static TokenBuffer *make_token_buffer(void)
{
    TokenBuffer *tok_buf = malloc(sizeof *tok_buf);
    if (!tok_buf)
    {
        fprintf(stderr, "I don't know why, but... we just couldn't allocate your token buffer... ");
        fprintf(stderr, "what's the fucking point...\n");
        exit(EXIT_FAILURE);
    }

    tok_buf->cap_grow = 8;
    tok_buf->capacity = tok_buf->cap_grow;
    tok_buf->len = 0;

    tok_buf->data = malloc(tok_buf->capacity * sizeof(Token));
    if (!tok_buf->data)
    {
        fprintf(stderr, "I have terrible news about your token buffer... ");
        fprintf(stderr, "We just couldn't allocate space for the data field... I'm so sorry...\n");
        exit(EXIT_FAILURE);
    }

    return tok_buf;
}

static void destroy_token_buffer(TokenBuffer *tok_buf)
{
    if (!tok_buf)
        return;
    free(tok_buf->data);
    free(tok_buf);
}

static void append_token_buffer(TokenBuffer *tok_buf, Token tok)
{
    tok_buf->data[tok_buf->len] = tok;
    tok_buf->len++;

    if (tok_buf->len >= tok_buf->capacity - 1)
    {
        tok_buf->capacity += tok_buf->cap_grow;
        Token *tmp = realloc(tok_buf->data, tok_buf->capacity * sizeof(Token));
        if (!tmp)
        {
            fprintf(stderr, "Can't grow your token buffer, sorry. Destroying everything.\n");
            exit(EXIT_FAILURE);
        }
        tok_buf->data = tmp;
    }
}

/*              LEXING              */
static void lex_error(void)
{
    printf("INVALID STRING! TRY AGAIN!\n");
}

static bool match_simple_token(char ch, TokenType *type)
{
    switch (ch)
    {
    case '(':
        *type = TOK_LPAREN;
        return true;
    case ')':
        *type = TOK_RPAREN;
        return true;
    case '+':
        *type = TOK_PLUS;
        return true;
    case '-':
        *type = TOK_MINUS;
        return true;
    case '*':
        *type = TOK_MULT;
        return true;
    case '/':
        *type = TOK_DIV;
        return true;
    case '^':
        *type = TOK_EXP;
        return true;
    default:
        return false;
    }
}

typedef enum num_state
{
    DIGIT_PRE_DECIMAL,
    DECIMAL_START,
    DECIMAL_MIDDLE,
    DIGIT_POST_DECIMAL
} NumState;

static void lex_number(char **ch, TokenBuffer *token_buffer)
{
    // Char must be [0-9] or . to enter this function
    int digit;
    double number = 0;
    double mult = 1;
    NumState state = -1;

    if (isdigit((unsigned char)**ch))
    {
        state = DIGIT_PRE_DECIMAL;
    }
    else if (**ch == '.')
    {
        state = DECIMAL_START;
        (*ch)++;
    }

    while (**ch != '\0')
    {
        switch (state)
        {
        case DIGIT_PRE_DECIMAL:
            if (isdigit((unsigned char)**ch))
            {
                digit = **ch - '0';
                number *= 10;
                number += digit;
                (*ch)++;
            }
            else if (**ch == '.')
            {
                state = DECIMAL_MIDDLE;
                (*ch)++;
            }
            else
            {
                append_token_buffer(token_buffer, make_number(number));
                return;
            }
            break;
        case DECIMAL_START:
            if (isdigit((unsigned char)**ch))
            {
                state = DIGIT_POST_DECIMAL;
            }
            else
            {
                lex_error();
            }
            break;
        case DECIMAL_MIDDLE:
            if (isdigit((unsigned char)**ch))
            {
                state = DIGIT_POST_DECIMAL;
            }
            else if (**ch == '.')
            {
                lex_error();
            }
            else
            {
                append_token_buffer(token_buffer, make_number(number));
                return;
            }
            break;
        case DIGIT_POST_DECIMAL:
            if (isdigit((unsigned char)**ch))
            {
                digit = **ch - '0';
                mult *= 0.1;
                number += (digit * mult);
                (*ch)++;
            }
            else if (**ch == '.')
            {
                lex_error();
            }
            else
            {
                append_token_buffer(token_buffer, make_number(number));
                return;
            }
            break;
        }
    }

    if (state == DECIMAL_START) // If we run out of input and there's only a . with no digits
    {
        lex_error();
    }

    append_token_buffer(token_buffer, make_number(number));
}

static TokenBuffer *lex_input(char *ch)
{
    TokenBuffer *token_buffer = make_token_buffer();

    while (*ch != '\0')
    {
        TokenType type;
        if (*ch == ' ') // Ignore whitespace and move on
        {
            ch++;
        }
        else if (match_simple_token(*ch, &type)) // ( ) + - / * or ^ get added here
        {
            append_token_buffer(token_buffer, make_token(type));
            ch++;
        }
        else if (isdigit((unsigned char)*ch) || *ch == '.') // Switch over to number lexer if it's [0-9] or .
        {
            lex_number(&ch, token_buffer);
        }
        else // Any unrecognized characters raise an error
        {
            lex_error();
            return NULL;
        }
    }

    return token_buffer;
}

/*              PARSING             */
typedef struct tree_node
{
    Token token;
    struct tree_node *left;
    struct tree_node *right;
} Tree;

static Tree *make_tree(Token token)
{
    Tree *tree = malloc(sizeof *tree);
    if (!tree)
        return NULL;
    tree->token = token;
    tree->left = NULL;
    tree->right = NULL;
    return tree;
}

static void destroy_tree(Tree *tree)
{
    if (!tree)
        return;
    destroy_tree(tree->left);
    destroy_tree(tree->right);
    free(tree);
}

static double eval_tree(Tree *tree)
{
    if (tree->token.type == TOK_NUMBER)
        return tree->token.number;

    switch (tree->token.type)
    {
    case TOK_DIV:
        return eval_tree(tree->left) / eval_tree(tree->right);
    case TOK_MULT:
        return eval_tree(tree->left) * eval_tree(tree->right);
    case TOK_PLUS:
        return eval_tree(tree->left) + eval_tree(tree->right);
    case TOK_MINUS:
        return eval_tree(tree->left) - eval_tree(tree->right);
    case TOK_EXP:
        return pow(eval_tree(tree->left), eval_tree(tree->right));
    default:
        abort();
    }
}

/*          Recursive descent parser        */
/* One function for each nonterminal symbol */

static Tree *parse_expr(TokenBuffer *tok_buf, size_t *peek);
static Tree *parse_term(TokenBuffer *tok_buf, size_t *peek);
static Tree *parse_unary(TokenBuffer *tok_buf, size_t *peek);
static Tree *parse_factor(TokenBuffer *tok_buf, size_t *peek);
static Tree *parse_base(TokenBuffer *tok_buf, size_t *peek);

static Tree *parse_base(TokenBuffer *tok_buf, size_t *peek)
{
    if (tok_buf->data[*peek].type == TOK_NUMBER)
    {
        Tree *num = make_tree(tok_buf->data[*peek]);
        (*peek)++;
        return num;
    }

    if (tok_buf->data[*peek].type == TOK_LPAREN)
    {
        (*peek)++; // Consume ( token
        Tree *expr = parse_expr(tok_buf, peek);
        if (tok_buf->data[*peek].type != TOK_RPAREN)
        {
            return NULL;
        }
        (*peek)++; // Consume ) token
        return expr;
    }

    return NULL;
}

static Tree *parse_factor(TokenBuffer *tok_buf, size_t *peek)
{
    Tree *base = parse_base(tok_buf, peek);
    if (!base)
        return NULL;
    if (tok_buf->data[*peek].type == TOK_EXP)
    {
        Tree *factor = make_tree(tok_buf->data[*peek]);
        (*peek)++;
        factor->left = base;
        factor->right = parse_unary(tok_buf, peek);
        if (!factor->right)
            return NULL;
        return factor;
    }
    return base;
}

static Tree *parse_unary(TokenBuffer *tok_buf, size_t *peek)
{
    if (tok_buf->data[*peek].type == TOK_MINUS)
    {
        (*peek)++;
        Tree *neg = make_tree(make_token(TOK_MULT));
        neg->left = make_tree(make_number(-1.0));
        neg->right = parse_unary(tok_buf, peek);
        if (!neg->right)
            return NULL;
        return neg;
    }
    Tree *factor = parse_factor(tok_buf, peek);
    if (!factor)
        return NULL;
    return factor;
}

static Tree *parse_term(TokenBuffer *tok_buf, size_t *peek)
{
    Tree *left = parse_unary(tok_buf, peek);
    if (!left)
        return NULL;
    while (tok_buf->data[*peek].type == TOK_MULT || tok_buf->data[*peek].type == TOK_DIV)
    {
        Tree *term = make_tree(tok_buf->data[*peek]);
        (*peek)++;
        term->left = left;
        term->right = parse_unary(tok_buf, peek);
        if (!term->right)
        {
            destroy_tree(term);
            return NULL;
        }
        left = term;
    }
    return left;
}

static Tree *parse_expr(TokenBuffer *tok_buf, size_t *peek)
{
    Tree *left = parse_term(tok_buf, peek);
    if (!left)
        return NULL;
    while (tok_buf->data[*peek].type == TOK_PLUS || tok_buf->data[*peek].type == TOK_MINUS)
    {
        Tree *expr = make_tree(tok_buf->data[*peek]);
        (*peek)++;
        expr->left = left;
        expr->right = parse_term(tok_buf, peek);
        if (!expr->right) // missing right operand
        {
            destroy_tree(expr);
            return NULL;
        }
        left = expr; // tree so far becomes left side of the next operator
    }
    return left;
}

static Tree *parse_syntax(TokenBuffer *tok_buf)
{
    /* Parses the expression, building a syntax tree using recursive descent */
    size_t peek = 0;
    Tree *ast = parse_expr(tok_buf, &peek);
    if (!ast)
        return NULL;
    if (peek != tok_buf->len) // Parser stopped early: leftover tokens means invalid syntax
    {
        destroy_tree(ast);
        return NULL;
    }
    return ast;
}

int main(void)
{
    while (true) // Calculator REPL
    {
        char *char_buf = read_input();

        TokenBuffer *token_buf = lex_input(char_buf);
        if (!token_buf)
        {
            free(char_buf);
            destroy_token_buffer(token_buf);
            continue;
        }

        Tree *ast = parse_syntax(token_buf);
        if (!ast)
        {
            printf("Invalid syntax! You really fucked up bad. I can't believe you tried entering that...\n");
            free(char_buf);
            destroy_token_buffer(token_buf);
            continue;
        }

        double result = eval_tree(ast);
        printf("%f\n", result);

        destroy_tree(ast);
        free(char_buf);
        destroy_token_buffer(token_buf);
    }

    return 0;
}