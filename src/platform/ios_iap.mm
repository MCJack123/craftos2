#import <StoreKit/StoreKit.h>
#include "../runtime.hpp"

static std::string products_response(lua_State *L, void *ud) {
    SKProductsResponse * res = (SKProductsResponse*)ud;
    lua_createtable(L, res.products.count, 0);
    for (int i = 0; i < res.products.count; i++) {
        SKProduct * product = res.products[i];
        lua_createtable(L, 0, 4);
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
        lua_rawseti(L, -2, i);
        [numberFormatter release];
        [formattedString release];
    }
    return "_CCPC_products_response";
}

@interface IAPObserver : NSObject<SKPaymentTransactionObserver, SKProductsRequestDelegate> {
    SKProductsRequest * request;
    SKProductsResponse * response = nil;
    Computer * computer;
}
@end
@property (retain, nonatomic) NSMutableSet<NSString *> * eligibleProducts;
@implementation IAPObserver

- (id)init {
    eligibleProducts = [[NSMutableSet alloc] init];
    NSArray * array = [[NSUserDefaults standardUserDefaults] arrayForKey:@"EligibleProducts"];
    if (array != nil) eligibleProducts = [NSSet setWithArray:array];
    else eligibleProducts = [NSSet set];
    return self;
}

- (BOOL)checkForProductEligibility:(NSString *)identifier {
    return [eligibleProducts containsObject:identifier];
}

- (void)validateProductIdentifiers:(NSArray *)productIdentifiers forComputer:(Computer *)comp {
    if (response != nil) {
        queueEvent(computer, products_response, products);
        return;
    }
    SKProductsRequest *productsRequest = [[SKProductsRequest alloc]
        initWithProductIdentifiers:[NSSet setWithArray:productIdentifiers]];

    // Keep a strong reference to the request.
    request = productsRequest;
    computer = comp;
    productsRequest.delegate = self;
    [productsRequest start];
}

- (BOOL)purchaseProduct:(NSString *)identifier {
    if (response == nil) return NO;
    SKProduct * product = nil;
    for (SKProduct * p in response.products) {
        if (p.productIdentifier == identifier) {
            product = p;
            break;
        }
    }
    if (product == nil) return NO;
    SKMutablePayment * payment = [SKMutablePayment paymentWithProduct:product];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
    return YES;
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
                break;
            } case SKPaymentTransactionStatePurchased:
              case SKPaymentTransactionStateRestored: {
                [eligibleProducts addObject:transaction.payment.productIdentifier];
                [[NSUserDefaults standardUserDefaults] setObject:eligibleProducts.allObjects forKey:@"EligibleProducts"];
                break;
            }
        }
    }
}

- (void)paymentQueue:(SKPaymentQueue *)queue removedTransactions:(NSArray<SKPaymentTransaction *> *)transactions {
    
}

- (void)paymentQueue:(SKPaymentQueue *)queue didRevokeEntitlementsForProductIdentifiers:(NSArray<NSString *> *)productIdentifiers {

}

// MARK: SKPaymentResponseDelegate

- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response {
    products = response.products;
    for (NSString * identifier in res.invalidProductIdentifiers) {
        NSLog(@"App Store reported invalid identifier for product %@", identifier);
    }
    queueEvent(computer, products_response, response);
}

@end