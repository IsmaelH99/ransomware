#include <openssl/evp.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
 
#define KEY_SIZE 32
#define IV_SIZE 16
 
void handleErrors(void) {
    ERR_print_errors_fp(stderr);
    abort();
}
 
void decrypt_file_in_place(const char *file_path, const unsigned char *decryption_key) {
    FILE *fp = fopen(file_path, "rb+");
    if (!fp) {
        perror("Impossible d'ouvrir le fichier pour le déchiffrement");
        return;
    }
 
    unsigned char iv[IV_SIZE];
    if (fread(iv, 1, IV_SIZE, fp) != IV_SIZE) {
        perror("Échec de lecture du vercteur d'initialisation");
        fclose(fp);
        return;
    }
 
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp) - IV_SIZE;
    rewind(fp);
    fseek(fp, IV_SIZE, SEEK_SET);
 
    unsigned char *ciphertext = malloc(filesize);
    if (!ciphertext) {
        perror("Échec d'allocation de mémoire pour le texte chiffré");
        fclose(fp);
        return;
    }
 
    if (fread(ciphertext, 1, filesize, fp) != filesize) {
        perror("Échec de lecture du texte chiffré");
        free(ciphertext);
        fclose(fp);
        return;
    }
 
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char *plaintext = malloc(filesize);
    int len, plaintext_len;
 
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, decryption_key, iv)) handleErrors();
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, filesize)) handleErrors();
    plaintext_len = len;
    if (!EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) handleErrors();
    plaintext_len += len;
 
    rewind(fp);
    fwrite(iv, 1, IV_SIZE, fp);
    fwrite(plaintext, 1, plaintext_len, fp);
    ftruncate(fileno(fp), plaintext_len + IV_SIZE);
 
    EVP_CIPHER_CTX_free(ctx);
    free(ciphertext);
    free(plaintext);
    fclose(fp);
}
 
void decrypt_directory_in_place(const char *dir_path, const unsigned char *decryption_key) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Échec d'ouverture du répertoire");
        return;
    }
 
    struct dirent *entry;
    char path[1024];
 
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue; 
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        struct stat path_stat;
        stat(path, &path_stat);
 
        if (S_ISDIR(path_stat.st_mode)) {
            decrypt_directory_in_place(path, decryption_key);
        } else {
            decrypt_file_in_place(path, decryption_key);
            printf("Déchiffré: %s\n", path);
        }
    }
    closedir(dir);
}
 
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <répertoire_à_déchiffrer> <clé>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
 
    unsigned char key[KEY_SIZE];
    FILE *key_file = fopen(argv[2], "rb");
    if (!key_file) {
        perror("Impossible d'ouvrir la clé");
        exit(EXIT_FAILURE);
    }
 
    if (fread(key, 1, KEY_SIZE, key_file) < KEY_SIZE) {
        fprintf(stderr, "Erreur lors de la lecture de la clé\n");
        fclose(key_file);
        exit(EXIT_FAILURE);
    }
    fclose(key_file);
 
    decrypt_directory_in_place(argv[1], key);
 
    printf("Le déchiffrement a été effectué avec succès.\n");
 
    return 0;
}