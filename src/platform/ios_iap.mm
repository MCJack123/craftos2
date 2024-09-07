/*
 * SPDX-FileCopyrightText: 2019-2024 JackMacWindows
 *
 * SPDX-License-Identifier: MIT
 */

#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>
#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs7err.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Crypto/X509Certificate.h>
#include "../runtime.hpp"
#include "../terminal/SDLTerminal.hpp"

#define BUFFERSIZE 4096

extern std::vector<Poco::Crypto::X509Certificate> certCache;
extern void loadCerts();

static std::string products_response(lua_State *L, void *ud);

static std::string purchase_complete(lua_State *L, void *ud) {
    lua_pushstring(L, (const char*)ud);
    return "_CCPC_purchase_complete";
}

static std::string purchase_failed(lua_State *L, void *ud) {
    lua_pushstring(L, (const char*)ud);
    return "_CCPC_purchase_failed";
}

// Copy of PKCS7_verify from OpenSSL, but with timestamp support
static int PKCS7_verify_timestamp(PKCS7 *p7, STACK_OF(X509) *certs, X509_STORE *store,
                 BIO *indata, BIO *out, int flags, time_t timestamp)
{
    STACK_OF(X509) *signers;
    X509 *signer;
    STACK_OF(PKCS7_SIGNER_INFO) *sinfos;
    PKCS7_SIGNER_INFO *si;
    X509_STORE_CTX *cert_ctx = NULL;
    char *buf = NULL;
    int i, j = 0, k, ret = 0;
    BIO *p7bio = NULL;
    BIO *tmpin = NULL, *tmpout = NULL;

    if (!p7) {
        PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_INVALID_NULL_POINTER);
        return 0;
    }

    if (!PKCS7_type_is_signed(p7)) {
        PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_WRONG_CONTENT_TYPE);
        return 0;
    }

    /* Check for no data and no content: no data to verify signature */
    if (PKCS7_get_detached(p7) && !indata) {
        PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_NO_CONTENT);
        return 0;
    }

    if (flags & PKCS7_NO_DUAL_CONTENT) {
        /*
         * This was originally "#if 0" because we thought that only old broken
         * Netscape did this.  It turns out that Authenticode uses this kind
         * of "extended" PKCS7 format, and things like UEFI secure boot and
         * tools like osslsigncode need it.  In Authenticode the verification
         * process is different, but the existing PKCs7 verification works.
         */
        if (!PKCS7_get_detached(p7) && indata) {
            PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_CONTENT_AND_DATA_PRESENT);
            return 0;
        }
    }

    sinfos = PKCS7_get_signer_info(p7);

    if (!sinfos || !sk_PKCS7_SIGNER_INFO_num(sinfos)) {
        PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_NO_SIGNATURES_ON_DATA);
        return 0;
    }

    signers = PKCS7_get0_signers(p7, certs, flags);
    if (!signers)
        return 0;

    /* Now verify the certificates */

    cert_ctx = X509_STORE_CTX_new();
    if (cert_ctx == NULL)
        goto err;
    if (!(flags & PKCS7_NOVERIFY))
        for (k = 0; k < sk_X509_num(signers); k++) {
            signer = sk_X509_value(signers, k);
            if (!(flags & PKCS7_NOCHAIN)) {
                if (!X509_STORE_CTX_init(cert_ctx, store, signer,
                                         p7->d.sign->cert)) {
                    PKCS7err(PKCS7_F_PKCS7_VERIFY, ERR_R_X509_LIB);
                    goto err;
                }
                X509_STORE_CTX_set_default(cert_ctx, "smime_sign");
            } else if (!X509_STORE_CTX_init(cert_ctx, store, signer, NULL)) {
                PKCS7err(PKCS7_F_PKCS7_VERIFY, ERR_R_X509_LIB);
                goto err;
            }
            if (!(flags & PKCS7_NOCRL))
                X509_STORE_CTX_set0_crls(cert_ctx, p7->d.sign->crl);
            X509_STORE_CTX_set_time(cert_ctx, 0, timestamp);
            i = X509_verify_cert(cert_ctx);
            if (i <= 0)
                j = X509_STORE_CTX_get_error(cert_ctx);
            X509_STORE_CTX_cleanup(cert_ctx);
            if (i <= 0) {
                PKCS7err(PKCS7_F_PKCS7_VERIFY,
                         PKCS7_R_CERTIFICATE_VERIFY_ERROR);
                ERR_add_error_data(2, "Verify error:",
                                   X509_verify_cert_error_string(j));
                goto err;
            }
            /* Check for revocation status here */
        }

    /*
     * Performance optimization: if the content is a memory BIO then store
     * its contents in a temporary read only memory BIO. This avoids
     * potentially large numbers of slow copies of data which will occur when
     * reading from a read write memory BIO when signatures are calculated.
     */

    if (indata && (BIO_method_type(indata) == BIO_TYPE_MEM)) {
        char *ptr;
        long len;
        len = BIO_get_mem_data(indata, &ptr);
        tmpin = (len == 0) ? indata : BIO_new_mem_buf(ptr, len);
        if (tmpin == NULL) {
            PKCS7err(PKCS7_F_PKCS7_VERIFY, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    } else
        tmpin = indata;

    if ((p7bio = PKCS7_dataInit(p7, tmpin)) == NULL)
        goto err;

    if (flags & PKCS7_TEXT) {
        if ((tmpout = BIO_new(BIO_s_mem())) == NULL) {
            PKCS7err(PKCS7_F_PKCS7_VERIFY, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        BIO_set_mem_eof_return(tmpout, 0);
    } else
        tmpout = out;

    /* We now have to 'read' from p7bio to calculate digests etc. */
    if ((buf = (char*)OPENSSL_malloc(BUFFERSIZE)) == NULL) {
        PKCS7err(PKCS7_F_PKCS7_VERIFY, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    for (;;) {
        i = BIO_read(p7bio, buf, BUFFERSIZE);
        if (i <= 0)
            break;
        if (tmpout)
            BIO_write(tmpout, buf, i);
    }

    if (flags & PKCS7_TEXT) {
        if (!SMIME_text(tmpout, out)) {
            PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_SMIME_TEXT_ERROR);
            BIO_free(tmpout);
            goto err;
        }
        BIO_free(tmpout);
    }

    /* Now Verify All Signatures */
    if (!(flags & PKCS7_NOSIGS))
        for (i = 0; i < sk_PKCS7_SIGNER_INFO_num(sinfos); i++) {
            si = sk_PKCS7_SIGNER_INFO_value(sinfos, i);
            signer = sk_X509_value(signers, i);
            j = PKCS7_signatureVerify(p7bio, p7, si, signer);
            if (j <= 0) {
                PKCS7err(PKCS7_F_PKCS7_VERIFY, PKCS7_R_SIGNATURE_FAILURE);
                goto err;
            }
        }

    ret = 1;

 err:
    X509_STORE_CTX_free(cert_ctx);
    OPENSSL_free(buf);
    if (tmpin == indata) {
        if (indata)
            BIO_pop(p7bio);
    }
    BIO_free_all(p7bio);
    sk_X509_free(signers);
    return ret;
}

@interface IAPObserver : NSObject<SKPaymentTransactionObserver, SKProductsRequestDelegate> {
    Computer * computer;
    X509_STORE * rootCert;
    BOOL validReceipt;
}
@property (retain, nonatomic) NSMutableSet<NSString *> * eligibleProducts;
@property (retain, nonatomic) SKProductsRequest * request;
@property (retain, nonatomic) SKReceiptRefreshRequest * refreshRequest;
@property (retain, nonatomic) SKProductsResponse * response;
@end

@implementation IAPObserver

- (id)init {
    [super init];
    self.response = nil;
    rootCert = NULL;
    computer = NULL;
    self.eligibleProducts = [NSMutableSet set];
    validReceipt = [self reloadReceipt];
    return self;
}
    
- (BOOL)reloadReceipt {
    if (rootCert == NULL) {
        rootCert = X509_STORE_new();
        if (certCache.empty()) loadCerts();
        for (Poco::Crypto::X509Certificate& cert : certCache) {
            if (cert.commonName().substr(0, 10) == "Apple Root") {
                X509_STORE_add_cert(rootCert, const_cast<X509*>(cert.certificate()));
            }
        }
    }
    
    NSURL *url = [NSBundle mainBundle].appStoreReceiptURL;
    if (url == nil) {
        NSLog(@"Receipt validation failed: No receipt URL found");
        return NO;
    }
    FILE *fp = fopen([url.path cStringUsingEncoding:NSUTF8StringEncoding], "rb");
    if (fp == NULL) {
        NSLog(@"Receipt validation failed: Could not open receipt");
        return NO;
    }
    PKCS7 *receipt = d2i_PKCS7_fp(fp, NULL);
    fclose(fp);
    BIO *payload = BIO_new(BIO_s_mem());
    if (!PKCS7_verify(receipt, NULL, rootCert, NULL, payload, PKCS7_NOVERIFY)) {
        NSLog(@"Receipt validation failed: Receipt signature validation failed");
        BIO_free(payload);
        PKCS7_free(receipt);
        return NO;
    }
    
    // Thanks to ChatGPT for writing some of this for me. I hate OpenSSL's lack of docs so much.
    
    const unsigned char* set_data = NULL;
    long set_length = 0;
    int set_tag = 0;
    int set_class = 0;
    int set_primitive = 0;

    // Get the set data and length
    set_primitive = !(ASN1_get_object((const unsigned char**)&set_data, &set_length, &set_tag, &set_class, BIO_get_mem_data(payload, &set_data)) & 0x20);

    // Check that the set is constructed
    if (set_primitive || set_tag != 0x11) {
        NSLog(@"Receipt validation failed: Expected constructed set, but found primitive set\n");
        BIO_free(payload);
        PKCS7_free(receipt);
        return NO;
    }

    // Parse the set as a sequence of sequences
    const unsigned char* sequence_data = set_data;
    long sequence_length = 0;
    int sequence_tag = 0;
    int sequence_class = 0;
    int sequence_primitive = 0;
    std::string hash, salt;
    time_t timestamp = 0;
    NSMutableArray * foundIdentifiers = [NSMutableArray array];

    while (sequence_data < set_data + set_length) {
        // Get the next sequence data and length
        sequence_primitive = ASN1_get_object((const unsigned char**)&sequence_data, &sequence_length, &sequence_tag, &sequence_class, set_length < 0 ? 0 : set_length);
        if (sequence_primitive & 0x80) break;

        // Check that the sequence is constructed
        if (!(sequence_primitive & 0x20) || sequence_tag != 0x10) {
            NSLog(@"Receipt validation failed: Expected constructed sequence, but found primitive sequence");
            BIO_free(payload);
            PKCS7_free(receipt);
            return NO;
        }

        ASN1_INTEGER *typeV = d2i_ASN1_INTEGER(NULL, &sequence_data, set_length < 0 ? 0 : set_length);
        ASN1_INTEGER *versionV = d2i_ASN1_INTEGER(NULL, &sequence_data, set_length < 0 ? 0 : set_length);
        ASN1_OCTET_STRING *valV = d2i_ASN1_OCTET_STRING(NULL, &sequence_data, set_length < 0 ? 0 : set_length);
        long type = ASN1_INTEGER_get(typeV);
        long version = ASN1_INTEGER_get(versionV);
        const unsigned char * val = valV->data;
        long val_len = valV->length;
        ASN1_INTEGER_free(typeV);
        ASN1_INTEGER_free(versionV);
        
        switch (type) {
            case 2: { // bundle identifier {UTF8STRING}
                ASN1_UTF8STRING *str = d2i_ASN1_UTF8STRING(NULL, &val, val_len);
                if (std::string((const char*)str->data, str->length) != "cc.craftos-pc.CraftOS-PC-iOS") {
                    NSLog(@"Receipt validation failed: Bundle identifier does not match");
                    ASN1_UTF8STRING_free(str);
                    ASN1_OCTET_STRING_free(valV);
                    BIO_free(payload);
                    PKCS7_free(receipt);
                    return NO;
                }
                ASN1_UTF8STRING_free(str);
                break;
            } case 3: { // version {UTF8STRING}
                ASN1_UTF8STRING *str = d2i_ASN1_UTF8STRING(NULL, &val, val_len);
                if (std::string((const char*)str->data, str->length) != [(NSString *)[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"] UTF8String]) {
                    NSLog(@"Receipt validation failed: Bundle version does not match");
                    ASN1_UTF8STRING_free(str);
                    ASN1_OCTET_STRING_free(valV);
                    BIO_free(payload);
                    PKCS7_free(receipt);
                    return NO;
                }
                ASN1_UTF8STRING_free(str);
                break;
            } case 4: { // salt {bytes}
                salt = std::string((const char*)val, val_len);
                break;
            } case 5: { // hash {bytes}
                hash = std::string((const char*)val, val_len);
                break;
            } case 12: { // creation date {IA5STRING as RFC 3339/ISO 8601 date}
                ASN1_IA5STRING *str = d2i_ASN1_IA5STRING(NULL, &val, val_len);
                int diff = 0;
                timestamp = Poco::DateTimeParser::parse("%Y-%m-%dT%H:%M:%S%z", std::string((const char*)str->data, str->length), diff).timestamp().epochTime();
                ASN1_IA5STRING_free(str);
                break;
            } case 17: { // IAP receipt {SET of attributes} - may have multiple
                long attr_set_length = 0;
                long attr_length = 0;
                int attr_set_tag = 0;
                int attr_set_class = 0;

                // Get the set data and length
                ASN1_get_object((const unsigned char**)&val, &attr_set_length, &attr_set_tag, &attr_set_class, val_len);
                
                while (val < valV->data + val_len) {
                    // Get the next sequence data and length
                    if (ASN1_get_object((const unsigned char**)&val, &attr_length, &attr_set_tag, &attr_set_class, attr_set_length < 0 ? 0 : attr_set_length) & 0x80) break;
                    
                    ASN1_INTEGER *attr_typeV = d2i_ASN1_INTEGER(NULL, &val, attr_set_length < 0 ? 0 : attr_set_length);
                    ASN1_INTEGER *attr_versionV = d2i_ASN1_INTEGER(NULL, &val, attr_set_length < 0 ? 0 : attr_set_length);
                    ASN1_OCTET_STRING *attr_valV = d2i_ASN1_OCTET_STRING(NULL, &val, attr_set_length < 0 ? 0 : attr_set_length);
                    long attr_type = ASN1_INTEGER_get(attr_typeV);
                    long attr_version = ASN1_INTEGER_get(attr_versionV);
                    const unsigned char * attr_val = attr_valV->data;
                    long attr_val_len = attr_valV->length;
                    ASN1_INTEGER_free(attr_typeV);
                    ASN1_INTEGER_free(attr_versionV);
                    
                    if (attr_type == 1702) {
                        ASN1_UTF8STRING *str = d2i_ASN1_UTF8STRING(NULL, &attr_val, attr_val_len);
                        [foundIdentifiers addObject:[NSString stringWithUTF8String:(const char*)str->data]];
                        ASN1_UTF8STRING_free(str);
                    }
                    ASN1_OCTET_STRING_free(attr_valV);
                    
                    attr_set_length -= attr_length;
                }
                break;
            } case 21: { // expiration date
                break;
            }
        }
        ASN1_OCTET_STRING_free(valV);
    }
    
    BIO_free(payload);
    
#ifndef DEBUG // Debug builds use self-signed certs
    if (!PKCS7_verify_timestamp(receipt, NULL, rootCert, NULL, NULL, 0, timestamp)) {
        NSLog(@"Receipt validation failed: Receipt certificate validation failed");
        PKCS7_free(receipt);
        return NO;
    }
#endif
    
    PKCS7_free(receipt);
    
#ifndef DEBUG // Debug builds use ?????
    unsigned char uuid_bytes[16], hash_out[20];
    [[UIDevice currentDevice].identifierForVendor getUUIDBytes:uuid_bytes];
    std::string id_data = std::string((const char*)uuid_bytes, 16) + salt + "\x0C\034cc.craftos-pc.CraftOS-PC-iOS";
    SHA1((const unsigned char*)id_data.c_str(), id_data.size(), hash_out);
    if (memcmp(hash.c_str(), hash_out, 20) != 0) {
        NSLog(@"Receipt validation failed: Invalid device hash");
        return NO;
    }
#endif
    
    for (NSString * str in foundIdentifiers) {
        [self.eligibleProducts addObject:str];
    }
    
    return YES;
}

- (void)dealloc {
    [super dealloc];
    if (rootCert != NULL) X509_STORE_free(rootCert);
}

- (BOOL)checkForProductEligibility:(NSString *)identifier {
    return [self.eligibleProducts containsObject:identifier];
}

- (void)validateProductIdentifiers:(NSArray *)productIdentifiers forComputer:(Computer *)comp {
    if (self.response != nil) {
        queueEvent(comp, products_response, self.response);
        return;
    }
    SKProductsRequest *productsRequest = [[SKProductsRequest alloc]
        initWithProductIdentifiers:[NSSet setWithArray:productIdentifiers]];

    // Keep a strong reference to the request.
    self.request = productsRequest;
    computer = comp;
    productsRequest.delegate = self;
    [productsRequest start];
}

- (BOOL)purchaseProduct:(NSString *)identifier forComputer:(Computer *)comp {
    if (self.response == nil) return NO;
    SKProduct * product = nil;
    for (SKProduct * p in self.response.products) {
        if ([p.productIdentifier isEqual:identifier]) {
            product = p;
            break;
        }
    }
    if (product == nil) return NO;
    SKMutablePayment * payment = [SKMutablePayment paymentWithProduct:product];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
    computer = comp;
    return YES;
}

- (void)restorePurchases:(Computer*)comp {
    computer = comp;
    if (!validReceipt) {
        self.refreshRequest = [[SKReceiptRefreshRequest alloc] init];
        self.refreshRequest.delegate = self;
        [self.refreshRequest start];
    } else {
        [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
    }
}

// MARK: SKRequestDelegate

- (void)requestDidFinish:(SKRequest *)request {
    if (request == self.refreshRequest) {
        validReceipt = [self reloadReceipt];
        queueTask([](void * computer)->void*{SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Restore Successful", "Your purchases have successfully been restored.", computer ? ((SDLTerminal*)((Computer*)computer)->term)->win : NULL); return NULL;}, computer, true);
        if (computer != NULL) queueEvent(computer, purchase_complete, (void*)"");
        computer = NULL;
    }
}

// MARK: SKPaymentTransactionObserver

- (void)paymentQueue:(SKPaymentQueue *)queue updatedTransactions:(NSArray<SKPaymentTransaction *> *)transactions {
    for (SKPaymentTransaction * transaction in transactions) {
        switch (transaction.transactionState) {
            case SKPaymentTransactionStatePurchasing: {
                break;
            } case SKPaymentTransactionStateDeferred: {
                break;
            } case SKPaymentTransactionStateFailed: {
                if (computer != NULL) queueEvent(computer, purchase_failed, (void*)transaction.payment.productIdentifier.UTF8String);
                computer = NULL;
                [queue finishTransaction:transaction];
                break;
            } case SKPaymentTransactionStatePurchased:
              case SKPaymentTransactionStateRestored: {
                [self.eligibleProducts addObject:transaction.payment.productIdentifier];
                [[NSUserDefaults standardUserDefaults] setObject:self.eligibleProducts.allObjects forKey:@"EligibleProducts"];
                if (computer != NULL) queueEvent(computer, purchase_complete, (void*)transaction.payment.productIdentifier.UTF8String);
                computer = NULL;
                [queue finishTransaction:transaction];
                break;
            }
        }
    }
}

- (void)paymentQueue:(SKPaymentQueue *)queue removedTransactions:(NSArray<SKPaymentTransaction *> *)transactions {
    
}

- (void)paymentQueue:(SKPaymentQueue *)queue didRevokeEntitlementsForProductIdentifiers:(NSArray<NSString *> *)productIdentifiers {

}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue {
    queueTask([](void * computer)->void*{SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Restore Successful", "Your purchases have successfully been restored.", computer ? ((SDLTerminal*)((Computer*)computer)->term)->win : NULL); return NULL;}, computer, true);
    computer = NULL;
}

// MARK: SKPaymentResponseDelegate

- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)res {
    self.response = res;
    for (NSString * identifier in res.invalidProductIdentifiers) {
        NSLog(@"App Store reported invalid identifier for product %@", identifier);
    }
    queueEvent(computer, products_response, res);
    computer = NULL;
}

@end

static IAPObserver * iapObserver = NULL;

std::string products_response(lua_State *L, void *ud) {
    SKProductsResponse * res = (SKProductsResponse*)ud;
    lua_createtable(L, res.products.count, 0);
    for (int i = 0; i < res.products.count; i++) {
        SKProduct * product = res.products[i];
        lua_createtable(L, 0, 5);
        lua_pushstring(L, [product.productIdentifier cStringUsingEncoding:NSISOLatin1StringEncoding]);
        lua_setfield(L, -2, "identifier");
        lua_pushstring(L, [product.localizedDescription cStringUsingEncoding:NSISOLatin1StringEncoding]);
        lua_setfield(L, -2, "description");
        lua_pushstring(L, [product.localizedTitle cStringUsingEncoding:NSISOLatin1StringEncoding]);
        lua_setfield(L, -2, "title");
        NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
        [numberFormatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
        [numberFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
        [numberFormatter setLocale:product.priceLocale];
        NSString *formattedString = [numberFormatter stringFromNumber:product.price];
        lua_pushstring(L, [formattedString cStringUsingEncoding:NSISOLatin1StringEncoding]);
        lua_setfield(L, -2, "price");
        lua_pushboolean(L, [iapObserver.eligibleProducts containsObject:product.productIdentifier]);
        lua_setfield(L, -2, "owned");
        lua_rawseti(L, -2, i + 1);
        [numberFormatter release];
        [formattedString release];
    }
    return "_CCPC_products_response";
}

void initIAP() {
    iapObserver = [[IAPObserver alloc] init];
    [[SKPaymentQueue defaultQueue] addTransactionObserver:iapObserver];
}

void queueGetIAPList(Computer * comp) {
    NSString *filePath = [[NSBundle mainBundle] pathForResource:@"AvailableIAPs" ofType:@"plist"];
    NSArray *availableIAPs = [NSArray arrayWithContentsOfFile:filePath];
    [iapObserver validateProductIdentifiers:availableIAPs forComputer:comp];
}

bool purchaseIAP(const char * name, Computer * comp) {
    return [iapObserver purchaseProduct:[NSString stringWithUTF8String:name] forComputer:comp] == YES;
}

void restorePurchases(Computer * comp) {
    [iapObserver restorePurchases:comp];
}

bool checkIAPEligibility(const char * identifier) {
    return [iapObserver checkForProductEligibility:[NSString stringWithUTF8String:identifier]] == YES;
}
