#include "fs.h"


bool SimpleFs::is_valid_inode_num(const int inumber) {
	return inumber > 0 && inumber <= superblock.ninodes;		
}


/**
* Carrega as informações de um inode dentro de um bloco de inodes
* @return true caso consiga acessar o inode, false caso contrário
*/
bool SimpleFs::inode_load(const int inumber, fs_inode* inode) {
	if (this->is_valid_inode_num(inumber) == false) {
		return false;
	}	
	const int blocknum = ((inumber - 1) / INODES_PER_BLOCK) + 1;
	const int inodenum = (inumber - 1) - (blocknum - 1) * INODES_PER_BLOCK;
	this->disk->read(blocknum, this->tmp_block.data);
	*inode = this->tmp_block.inode[inodenum];
	return true;
}

/**
* Salva as informações de um inode
* @return true caso consiga salvar o inode, false caso contrário
*/
bool SimpleFs::inode_save(const int inumber, const fs_inode& inode) {
	if (this->is_valid_inode_num(inumber) == false) {
		return false;
	}	
	const int blocknum = ((inumber - 1) / INODES_PER_BLOCK) + 1;
	const int inodenum = (inumber - 1) - (blocknum - 1) * INODES_PER_BLOCK;
	this->disk->read(blocknum, this->tmp_block.data);
	*(this->tmp_block.inode + inodenum) = inode;
	this->disk->write(blocknum, this->tmp_block.data);
	return true;
}

/**
* Verifica se o número do bloco é de um bloco de dados válido.
* Um número de bloco de dado válido é um número maior que a quantidade de superblocos +
* quantidade de blocos de inodes e menor que a quantidade de blocos total
 */
bool SimpleFs::is_valid_datablock(const int idatablock) {	
	return idatablock > this->superblock.ninodeblocks && idatablock < this->superblock.nblocks;
}

/**
* Verifica se o número do bloco é de um bloco válido.
* Um número de bloco válido é um número maior que a quantidade de superblocos e
* menor que a quantidade de blocos total
 */
bool SimpleFs::is_valid_block(const int iblock) {
	return iblock >= 1 && iblock < this->superblock.nblocks;	
}

/**
* Limpa todos os dados armazenados em um bloco
 */
void SimpleFs::clear_block_data(const int iblock) {
	this->disk->write(iblock, this->EMPTY_BLOCK.data);
}


void SimpleFs::read_superblock() {
	this->disk->read(0, this->tmp_block.data);
	this->superblock = this->tmp_block.super;
}

/**
* Libera o bloco como livre no bitmap de blocos livres
 */
void SimpleFs::mark_block_free(const int iblock) {
	if (this->is_valid_block(iblock)) {
		this->bitmap[iblock] = false;
	}	
}

/*
* Marca o bloco como ocupado no bitmap de blocos livres
*/
void SimpleFs::mark_block_busy(const int iblock) {
	if (this->is_valid_block(iblock)) {
		this->bitmap[iblock] = true;
	}
}


/**
* Define como ocupados o superblock e os inodeblocks e como livres o restante
 */
void SimpleFs::reset_bitmap() {
	this->bitmap[0] = true;
	for (int i = 1; i < this->superblock.nblocks; i++) {
		i <= this->superblock.ninodeblocks ? this->mark_block_busy(i) : this->mark_block_free(i);		
	}
}


/**
* Tenta alocar um bloco de dados livre.
* @return O número do bloco de dados ou -1 caso o disco esteja cheio
 */
int SimpleFs::alloc_datablock() {	
	for (std::size_t i = 0; i < this->bitmap.size(); i++) {
		if (this->bitmap[i] == false) {
			this->bitmap[i] = true;
			return i;
		}
	}
	return -1;
}

/**
* Preenche, de forma sequêncial, um vetor com todos os blocos associados ao inode
 */
void SimpleFs::fill_datablocks(const int inumber, fs_datablock_vec_t& vec) {
	vec.indirect = 0;
	vec.blocks.clear();	

	fs_inode inode{};
	if (this->inode_load(inumber, &inode) == false) {
		return;
	}

	for (int i = 0; i < POINTERS_PER_INODE; i++) {
		if (this->is_valid_datablock(inode.direct[i])) {
			vec.blocks.push_back(inode.direct[i]);			
		}
	}
	
	if (this->is_valid_datablock(inode.indirect)) {
		this->disk->read(inode.indirect, this->tmp_block.data);
		vec.indirect = inode.indirect;
		for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
			if (this->is_valid_datablock(this->tmp_block.pointers[i]) == false) {
				return;
			}
			vec.blocks.push_back(this->tmp_block.pointers[i]);			
		}
	}	
}


bool SimpleFs::is_disk_full() const {
	for (bool b : this->bitmap)
		if (b == false)
			return false;
	return true;
}


/**
* Cria um novo sistema de arquivos no disco, destruindo qualquer dado que estiver presente. Reserva
* dez por cento dos blocos para inodos, libera a tabela de inodos, e escreve o superbloco. Retorna um para
* sucesso, zero caso contrário. Note que formatar o sistema de arquivos não faz com que ele seja montado.
* Também, uma tentativa de formatar um disco que já foi montado não deve fazer nada e retornar falha.
*/
int SimpleFs::fs_format()
{
	if (this->is_disk_mounted == true) {
		cout << "[ERROR] [DISK IS MOUNTED]\n";
		return 0;
	}

	fs_block block{};		
	block.super.magic = FS_MAGIC;
	block.super.nblocks = this->disk->size();
	block.super.ninodeblocks = std::round((this->disk->size() * 0.10f) + 0.5f);
	block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;	
	this->disk->write(0, block.data);

	for (int i = 1; i < this->disk->size(); i++) {
		this->clear_block_data(i);		
	}	

	return 1;
}

/**
* Varre um sistema de arquivos montado e reporta como os inodos e os blocos estão organizados.
*/
void SimpleFs::fs_debug()
{	
	if (this->is_disk_mounted == false) {
		cout << "[DISK NOT MOUNTED!]\n";
		return;
	}

	cout << "superblock:\n";
	cout << "    " << (this->superblock.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << this->superblock.nblocks << " blocks\n";
	cout << "    " << this->superblock.ninodeblocks << " inode blocks\n";
	cout << "    " << this->superblock.ninodes << " inodes\n";

	fs_block block{};

	for (int i = 1; i <= this->superblock.ninodeblocks; i++) {
		disk->read(i, this->tmp_block.data);

		for (int j = 0; j < INODES_PER_BLOCK; j++) {
			const fs_inode* inode = this->tmp_block.inode + j;
			if (inode->isvalid == 0) {
				continue;
			}

			cout << "inode: " << 1 + j + (i - 1) * INODES_PER_BLOCK << '\n';
			cout << "    " << "size: " << inode->size << " bytes\n";
			
			cout << "    " << "direct blocks:";
			for (int n = 0; n < POINTERS_PER_INODE; n++) {
				if (inode->direct[n] > 0) {
					cout << ' ' << inode->direct[n];
				}
			}
			cout << '\n';

			if (inode->indirect <= 0) {
				continue;
			}
		
			this->disk->read(inode->indirect, block.data);
			cout << "    " << "indirect block: " << inode->indirect << '\n';
			cout << "    " << "indirect data blocks:";
			for (int n = 0; n < POINTERS_PER_BLOCK; n++) {
				if (block.pointers[n] > 0) {
					cout << ' ' << block.pointers[n];
				}
			}
			cout << '\n';
		}
	}
}


/**
* Examina o disco para um sistema de arquivos. Se um está presente, lê o superbloco, constroi um
* bitmap de blocos livres, e prepara o sistema de arquivos para uso. Retorna um em caso de sucesso, zero
* caso contrário. Note que uma montagem bem-sucedida é um pré-requisito para as outras chamadas.
*/
int SimpleFs::fs_mount()
{	
	if (this->is_disk_mounted) {		
		return 1;
	}
	
	this->read_superblock();
	
	if (this->superblock.magic != FS_MAGIC) {
		cout << "[INVALID MAGIC NUM]\n";
		return 0;
	}

	this->reset_bitmap();

	std::vector<int> indirect_blocks{};
	
	// Lê ponteiros diretos
	for (int i = 1; i <= this->superblock.ninodeblocks; i++) {
		this->disk->read(i, this->tmp_block.data);		
		for (int j = 0; j < INODES_PER_BLOCK; j++) {
			const fs_inode* inode = this->tmp_block.inode + j;

			if (inode->isvalid == 0) {
				continue;
			}
					
			for (int n = 0; n < POINTERS_PER_INODE; n++) {
				this->mark_block_busy(inode->direct[n]);				
			}

			// Ponteiro indireto
			if (this->is_valid_datablock(inode->indirect)) {
				indirect_blocks.push_back(inode->indirect);
			}
		}
	}

	// Lê os ponteiros indiretos
	for (int p : indirect_blocks) {
		this->disk->read(p, this->tmp_block.data);
		this->mark_block_busy(p);
		for (int j = 0; j < POINTERS_PER_BLOCK; j++) {
			this->mark_block_busy(this->tmp_block.pointers[j]);
		}
	}

	this->is_disk_mounted = true;
	return 1;	
}


/**
* Cria um novo inodo de comprimento zero. Em caso de sucesso, retorna o inúmero (positivo). Em
* caso de falha, retorna zero. (Note que isto implica que zero não pode ser um inúmero válido.)
*/
int SimpleFs::fs_create()
{
	if (this->is_disk_mounted == false) {
		cout << "[DISK NOT MOUNTED]\n";
		return 0;
	}	
	
	for (int i = 1; i <= this->superblock.ninodeblocks; i++) {
		this->disk->read(i, this->tmp_block.data);
		for (int j = 0; j < INODES_PER_BLOCK; j++) {			
			if (this->tmp_block.inode[j].isvalid == 0) {
				const int inodenum = 1 + (j + (i - 1) * INODES_PER_BLOCK);				
				fs_inode inode = {};
				inode.isvalid = 1;
				this->inode_save(inodenum, inode);
				return inodenum;
			}
		}
	}

	return 0;
}


/**
* Deleta o inodo indicado pelo inúmero. Libera todo o dado e blocos indiretos atribuı́dos a este
* inodo e os retorna ao mapa de blocos livres. Em caso de sucesso, retorna um. Em caso de falha, retorna
* 0.
*/
int SimpleFs::fs_delete(const int inumber)
{	
	if (this->is_disk_mounted == false) {
		cout << "[DISK NOT MOUNTED]\n";
		return 0;
	}

	fs_inode inode{};
	if (this->inode_load(inumber, &inode) == false) {
		return 0;
	}

	fs_datablock_vec_t vec{};
	this->fill_datablocks(inumber, vec);

	// Free pointers block
	if (this->is_valid_datablock(vec.indirect)) {
		this->mark_block_free(vec.indirect);
		this->disk->write(vec.indirect, this->EMPTY_BLOCK.data);
	}

	// Free data blocks
	for (int datablock : vec.blocks) {
		this->mark_block_free(datablock);
		this->disk->write(datablock, this->EMPTY_BLOCK.data);
	}

	inode = fs_inode{};
	this->inode_save(inumber, inode);
	return 1;
}


/**
* Retorna o tamanho lógico do inodo especificado, em bytes. Note que zero é um tamanho lógico
* válido para um inodo! Em caso de falha, retorna -1.
*/
int SimpleFs::fs_getsize(const int inumber)
{
	if (this->is_disk_mounted == false) {
		cout << "[DISK NOT MOUNTED]!\n";
		return -1;
	}
	fs_inode inode{};
	if (this->inode_load(inumber, &inode) == false) {
		cout << "[INVALID INUMBER] [INUMBER -> " << inumber << "]\n";
		return -1;
	}
	return inode.size;
}


/**
* Lê um bloco de dados começando em start
* @return o número de bytes lidos
 */
int SimpleFs::read_datablock(
	const int idatablock,
	char* dest,
	int start,
	const int end
) {	
	if (this->is_valid_datablock(idatablock) == false) {
		return 0;
	}	

	int bytes = 0;
	// Caso start não seja um múltipo de Disk::DISK_BLOCK_SIZE então deve começar
	// a ler src em um número != 0
	int i = start % Disk::DISK_BLOCK_SIZE;
	char src[Disk::DISK_BLOCK_SIZE];
	this->disk->read(idatablock, src);

	while (i < Disk::DISK_BLOCK_SIZE && start < end && src[i] != '\0') {
		dest[start++] = src[i++];
		bytes++;
	}

	return bytes;
}


/**
* Lê dado de um inodo válido. Copia “length” bytes do inodo para dentro do ponteiro “data”,
* começando em “offset” no inodo. Retorna o número total de bytes lidos. O Número de bytes efetivamente
* lidos pode ser menos que o número de bytes requisitados, caso o fim do inodo seja alcançado. Se o inúmero
* dado for inválido, ou algum outro erro for encontrado, retorna 0.
*/
int SimpleFs::fs_read(int inumber, char *data, int length, int offset)
{
	if (this->is_disk_mounted == false) {
		cout << "[DISK NOT MOUNTED]\n";
		return 0;
	}

	fs_inode inode{};
	if (this->inode_load(inumber, &inode) == false) {
		cout << "[INVALID INODE] [INumber -> " << inumber << "]\n";
		return 0;
	}

	int bytes = 0;
	const int block_offset = offset / Disk::DISK_BLOCK_SIZE;	
	const int byte_offset = offset % Disk::DISK_BLOCK_SIZE;
		
	fs_datablock_vec_t vec{};	
	this->fill_datablocks(inumber, vec);

	for (std::size_t i = block_offset; i < vec.blocks.size() && bytes < length; i++) {
		const int n = this->read_datablock(
			vec.blocks[i], 
			data, 
			bytes + byte_offset, 
			length
		);
		bytes += n;
		if (n == 0) {
			break;
		}
	}

	return bytes;
}


/**
* Escreve um bloco de dados começando em start
* @return o número de bytes escritos
 */
int SimpleFs::write_datablock(
	const int idatablock,
	const char* src,
	int start,
	const int end
) {
	if (idatablock == 0) {
		return 0;
	}

	int bytes = 0;
	int i = start % Disk::DISK_BLOCK_SIZE;
	char dest[Disk::DISK_BLOCK_SIZE];

	this->disk->read(idatablock, dest);

	while (i < Disk::DISK_BLOCK_SIZE && start < end && src[start] != '\0') {
		dest[i++] = src[start++];
		bytes++;
	}

	this->disk->write(idatablock, dest);
	return bytes;
}


/**
* Escreve dado para um inodo válido. Copia “length” bytes do ponteiro “data” para o inodo
* começando em “offset” bytes. Aloca quaisquer blocos diretos e indiretos no processo. Retorna o número
* de bytes efetivamente escritos. O número de bytes efetivamente escritos pode ser menor que o número de
* bytes requisitados, caso o disco se torne cheio. Se o inúmero dado for inválido, ou qualquer outro erro for
* encontrado, retorna 0.
*/
int SimpleFs::fs_write(int inumber, const char *data, int length, int offset)
{
	if (this->is_disk_mounted == false) {
		cout << "[DISK NOT MOUNTED]\n";
		return 0;
	}
	
	if (length <= 0) {
		return 0;
	}

	if (this->is_disk_full()) {
		cout << "[DISK FULL]\n";
		return 0;
	}

	fs_inode inode{};
	if (this->inode_load(inumber, &inode) == false) {
		cout << "[INVALID INODE] [INUMBER -> " << inumber << "]\n";		
		return 0;
	}	
	
	int bytes = 0;
	const int block_offset = offset / Disk::DISK_BLOCK_SIZE;	
	const int byte_offset = offset % Disk::DISK_BLOCK_SIZE;
	
	fs_datablock_vec_t vec{};
	this->fill_datablocks(inumber, vec);
	
	int i = 0;
	while (bytes < length) {
		// Caso seja necessário alocar um bloco de ponteiros indiretos
		if (vec.indirect == 0 && vec.blocks.size() >= POINTERS_PER_INODE) {
			vec.indirect = this->alloc_datablock();
		}
		if (this->is_disk_full()) {
			cout << "[DISK IS FULL]\n";
			break;
		}
		// Aloca um bloco de dados para escrever
		const int datablock_num = this->alloc_datablock();
		vec.blocks.push_back(datablock_num);
		// Escreve no bloco alocado
		const int n = this->write_datablock(
			vec.blocks[block_offset + i++], 
			data, 
			bytes + byte_offset, 
			length
		);

		if (n == 0) {
			break;
		}		
		bytes += n;
	}
	
	// Atualiza o ponteiro para bloco de ponteiros indiretos
	inode.indirect = vec.indirect;

	// Atualiza os ponteiros diretos
	for (std::size_t i = 0; i < POINTERS_PER_INODE && i < vec.blocks.size(); i++) {
		inode.direct[i] = vec.blocks[i];
	}
	// Atualiza o bloco de ponteiros indiretos
	if (this->is_valid_datablock(inode.indirect)) {
		this->disk->read(inode.indirect, this->tmp_block.data);
		for (std::size_t i = POINTERS_PER_INODE; i < vec.blocks.size(); i++) {
			this->tmp_block.pointers[i - POINTERS_PER_INODE] = vec.blocks[i];
		}
		this->disk->write(inode.indirect, this->tmp_block.data);
	}

	// Atualiza tamanho do inode e salva
	inode.size += bytes;
	this->inode_save(inumber, inode);
	return bytes;
}
