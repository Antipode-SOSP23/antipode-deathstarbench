package tls

import  (
    "crypto/tls"
    "crypto/x509"
    "fmt"
    "io/ioutil"
    "log"
    "os"
    "strings"

    "google.golang.org/grpc"
    "google.golang.org/grpc/credentials"
)


var (
    dialopt grpc.DialOption
    serveropt grpc.ServerOption
    cipherSuites = map[string]uint16 {
        // TLS 1.0 - 1.2 cipher suites.
        "TLS_RSA_WITH_RC4_128_SHA": tls.TLS_RSA_WITH_RC4_128_SHA,
        "TLS_RSA_WITH_3DES_EDE_CBC_SHA": tls.TLS_RSA_WITH_3DES_EDE_CBC_SHA,
        "TLS_RSA_WITH_AES_128_CBC_SHA": tls.TLS_RSA_WITH_AES_128_CBC_SHA,
        "TLS_RSA_WITH_AES_256_CBC_SHA": tls.TLS_RSA_WITH_AES_256_CBC_SHA,
        "TLS_RSA_WITH_AES_128_CBC_SHA256": tls.TLS_RSA_WITH_AES_128_CBC_SHA256,
        "TLS_RSA_WITH_AES_128_GCM_SHA256": tls.TLS_RSA_WITH_AES_128_GCM_SHA256,
        "TLS_RSA_WITH_AES_256_GCM_SHA384": tls.TLS_RSA_WITH_AES_256_GCM_SHA384,

        "TLS_ECDHE_RSA_WITH_RC4_128_SHA": tls.TLS_ECDHE_RSA_WITH_RC4_128_SHA,
        "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA": tls.TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
        "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA": tls.TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA": tls.TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256": tls.TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256": tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384": tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256": tls.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305,

        // our certficate doesn't support ECDSA
        /*
        "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA": tls.TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
        "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA": tls.TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
        "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA": tls.TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
        "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256": tls.TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
        "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256": tls.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384": tls.TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256": tls.TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
        */

        // TLS 1.3 cipher suites.
        /* Not supported by golang v1.9
        "TLS_AES_128_GCM_SHA256": tls.TLS_AES_128_GCM_SHA256,
        "TLS_AES_256_GCM_SHA384": tls.TLS_AES_256_GCM_SHA384,
        "TLS_CHACHA20_POLY1305_SHA256": tls.TLS_CHACHA20_POLY1305_SHA256,
        */
    }
)

func checkTLS() (bool, string) {
    if val, ok := os.LookupEnv("TLS"); ok {
        if ( strings.EqualFold(val, "false") || strings.EqualFold(val, "0") ) {
            return false, ""
        }
        if ( strings.EqualFold(val, "true") || strings.EqualFold(val, "1") ) {
            return true, ""
        }

        if _, ok := cipherSuites[val]; ok {
            return true, val
        } else {
            log.Printf("WARNING: specified TLS cipher suite %s is invalid or unimplemented.\n", val)
            validCiphers := make([]string, 0, len(cipherSuites))
            for k := range(cipherSuites) {
                validCiphers = append(validCiphers, k)
            }
            log.Printf("WARNING: Please use the supported TLS cipher suite %s\n", validCiphers)
            return false, ""
        }
    }
    return false, ""
}

func init() {
    needTLS, cipher := checkTLS()
    if (needTLS) {
        b, err := ioutil.ReadFile("x509/ca_cert.pem")
	    if err != nil {
		    panic(err)
	    }
	    cp := x509.NewCertPool()
	    if !cp.AppendCertsFromPEM(b) {
		    panic(fmt.Errorf("credentials: failed to append certificates"))
        }
        config := tls.Config{
            ServerName: "x.test.example.com",
            RootCAs: cp,
        }
        if cipher != "" {
            log.Printf("TLS enabled cipher suite %s\n", cipher)
            config.CipherSuites = append(config.CipherSuites, cipherSuites[cipher])
        } else {
            log.Println("TLS enabled without specified cipher suite")
        }

        var creds credentials.TransportCredentials
        creds = credentials.NewTLS(&config)
        dialopt = grpc.WithTransportCredentials(creds)

        creds, err = credentials.NewServerTLSFromFile("x509/server_cert.pem", "x509/server_key.pem")
        if err != nil {
            panic(fmt.Sprintf("failed to create credentials: %v", err))
        }
        serveropt = grpc.Creds(creds)
    } else {
        log.Println("TLS disabled")
        dialopt = nil
        serveropt = nil
    }
}


func GetDialOpt() grpc.DialOption {
    return dialopt
}


func GetServerOpt() grpc.ServerOption {
    return serveropt
}
