package types

import (
	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/rlp"
)

/*
The special address used for registration.
Similar logic for the zero address used for cointract creation
 */

var	(
	RegAddr = [20]byte{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}
	EmptyAddr = [20]byte{0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00 }
	FakeSignature = [5]byte{0x00, 0x01, 0x02, 0x03, 0x04}
	)

type Registratoin struct{
	Account common.Address
	Referee common.Address

	IpStr string
	PortStr string

	Signature []byte //The signature of the Referee
	Renew uint64
}

type ConfirmBlockMsg struct {
	BlockNumber uint64
	Hash        common.Hash
	Confidence  uint64
	Supporters  []common.Address
}

type QueryBlockMsg struct{
	BlockNumber uint64
	Version uint64
	EmptyList []uint64
}


type Registrations []*Registratoin

func (r Registrations) Len() int {return len(r)}

func (r Registrations) GetRlp (i int) []byte {
	enc, _ := rlp.EncodeToBytes(r[i])
	return enc
}

